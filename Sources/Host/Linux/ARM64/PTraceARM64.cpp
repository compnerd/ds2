//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Host/Linux/PTrace.h"
#include "DebugServer2/Host/Linux/ExtraWrappers.h"
#include "DebugServer2/Host/Platform.h"

#include <algorithm>
#include <asm/ptrace.h>
#include <cstddef>
#include <cstring>
#include <elf.h>
#include <sys/ptrace.h>
#include <sys/uio.h>

#define super ds2::Host::POSIX::PTrace

namespace ds2 {
namespace Host {
namespace Linux {

namespace {
// The lowest DEBUG_ARCH value that implements the self-hosted debug
// register interface (NT_ARM_HW_BREAK/NT_ARM_HW_WATCH) this code relies on,
// introduced with ARMv8-A. Later architecture revisions (ARMv8.1 and
// beyond) report higher DEBUG_ARCH values here but remain backward
// compatible with this same register interface, so they should be accepted
// too rather than treated as unsupported.
constexpr uint32_t kMinDebugArch = 0x06;
}

int PTrace::getMaxStoppoints(ProcessThreadId const &ptid, int regSet) {
  // Retrieve the information about Hardware Breakpoint, if supported.
  // user_hwdebug_state.dbg_info is formatted as follows:
  //
  // 31             24             16               8              0
  // +---------------+--------------+---------------+---------------+
  // |   RESERVED    |   RESERVED   |   DEBUG_ARCH  |  NUM_SLOTS    |
  // +---------------+--------------+---------------+---------------+
  //
  // Deliberately do not go through readRegisterSet(): it DS2ASSERTs that the
  // kernel returns exactly as many bytes as were requested, but on real
  // hardware implementing fewer than the architectural 16 slots the kernel
  // shortens the note to just the implemented slots, which is shorter than
  // sizeof(drs) whenever fewer than 16 slots are implemented (the common
  // case in practice; see readHardwareWatchpointControl() below for the
  // same accommodation).
  struct user_hwdebug_state drs;
  std::memset(&drs, 0, sizeof(drs));

  struct iovec iov = {&drs, sizeof(drs)};
  if (wrapPtrace(PTRACE_GETREGSET, ptid.validTid() ? ptid.tid : ptid.pid,
                regSet, &iov) < 0 ||
      ((drs.dbg_info >> 8) & 0xff) < kMinDebugArch) {
    return 0;
  }

  return drs.dbg_info & 0xff;
}

int PTrace::getMaxHardwareBreakpoints(ProcessThreadId const &ptid) {
  return getMaxStoppoints(ptid, NT_ARM_HW_BREAK);
}

int PTrace::getMaxHardwareWatchpoints(ProcessThreadId const &ptid) {
  return getMaxStoppoints(ptid, NT_ARM_HW_WATCH);
}

ErrorCode PTrace::readHardwareWatchpointControl(
    ProcessThreadId const &ptid, std::vector<uint64_t> &addresses,
    std::vector<uint32_t> &controls) {
  struct user_hwdebug_state state;
  std::memset(&state, 0, sizeof(state));

  // Deliberately do not go through the shared readRegisterSet() helper here:
  // it DS2ASSERTs that the kernel returns exactly as many bytes as were
  // requested, but the kernel only ever fills in
  // offsetof(dbg_regs) + (dbg_info & 0xff) * sizeof(dbg_regs[0]) bytes of
  // this note -- i.e. exactly the number of watchpoints the target actually
  // implements, which on real hardware is almost always fewer than
  // array_sizeof(state.dbg_regs) (architecturally up to 16, but most cores
  // implement 2-6). Requesting the full 16-slot struct is intentional (it's
  // the only way to size a buffer without already knowing the count), so
  // that mismatch is expected, not an error.
  int regSet = NT_ARM_HW_WATCH;
  struct iovec iov = {&state, sizeof(state)};
  if (wrapPtrace(PTRACE_GETREGSET,
                ptid.validTid() ? ptid.tid : ptid.pid, regSet, &iov) < 0) {
    return Platform::TranslateError();
  }

  if (((state.dbg_info >> 8) & 0xff) < kMinDebugArch) {
    // Unsupported (pre-ARMv8-A) debug architecture; treat as "no
    // watchpoints available" rather than handing back (possibly
    // meaningless) register contents.
    addresses.clear();
    controls.clear();
    return kSuccess;
  }

  size_t count = std::min<size_t>(state.dbg_info & 0xff,
                                  array_sizeof(state.dbg_regs));

  // Do not read past what the kernel actually populated (see above).
  size_t regsOffset = offsetof(struct user_hwdebug_state, dbg_regs);
  size_t available =
      iov.iov_len > regsOffset
          ? (iov.iov_len - regsOffset) / sizeof(state.dbg_regs[0])
          : 0;
  count = std::min(count, available);

  addresses.resize(count);
  controls.resize(count);
  for (size_t i = 0; i < count; ++i) {
    addresses[i] = state.dbg_regs[i].addr;
    controls[i] = state.dbg_regs[i].ctrl;
  }

  return kSuccess;
}

ErrorCode PTrace::writeHardwareWatchpointControl(
    ProcessThreadId const &ptid, std::vector<uint64_t> const &addresses,
    std::vector<uint32_t> const &controls) {
  DS2ASSERT(addresses.size() == controls.size());

  struct user_hwdebug_state state;
  std::memset(&state, 0, sizeof(state));

  size_t count =
      std::min(addresses.size(), array_sizeof(state.dbg_regs));
  for (size_t i = 0; i < count; ++i) {
    state.dbg_regs[i].addr = addresses[i];
    state.dbg_regs[i].ctrl = controls[i];
  }

  // Only claim to write as many registers as the caller supplied (which
  // should always be exactly what readHardwareWatchpointControl() reported
  // as supported); the kernel's SETREGSET handler expects the note to be
  // sized for the number of debug registers it implements.
  size_t length = offsetof(struct user_hwdebug_state, dbg_regs) +
                  count * sizeof(state.dbg_regs[0]);
  return writeRegisterSet(ptid, NT_ARM_HW_WATCH, &state, length);
}

ErrorCode PTrace::readCPUState(ProcessThreadId const &ptid,
                               ProcessInfo const &pinfo,
                               Architecture::CPUState &state) {
  state.isA32 = pinfo.pointerSize == sizeof(uint32_t);

  // Read GPRs.
  struct user_pt_regs gprs;
  CHK(readRegisterSet(ptid, NT_PRSTATUS, &gprs, sizeof(gprs)));
  // The layout is identical.
  std::memcpy(state.state64.gp.regs, &gprs, sizeof(state.state64.gp.regs));

  return kSuccess;
}

ErrorCode PTrace::writeCPUState(ProcessThreadId const &ptid,
                                ProcessInfo const &,
                                Architecture::CPUState const &state) {
  // Write GPRs.
  struct user_pt_regs gprs;
  // The layout is identical.
  std::memcpy(&gprs, state.state64.gp.regs, sizeof(state.state64.gp.regs));
  CHK(writeRegisterSet(ptid, NT_PRSTATUS, &gprs, sizeof(gprs)));

  return kSuccess;
}
} // namespace Linux
} // namespace Host
} // namespace ds2
