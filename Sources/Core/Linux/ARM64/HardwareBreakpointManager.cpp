//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Architecture/ARM64/HardwareWatchpoint.h"
#include "DebugServer2/Core/HardwareBreakpointManager.h"
#include "DebugServer2/Host/Linux/ExtraWrappers.h"
#include "DebugServer2/Target/Process.h"
#include "DebugServer2/Target/Thread.h"

#include <csignal>

namespace ds2 {

//
// Linux: DBGWVRn_EL1/DBGWCRn_EL1 are accessed via
// PTRACE_GETREGSET/SETREGSET with NT_ARM_HW_WATCH (Host::Linux::PTrace
// wraps this as readHardwareWatchpointControl()/writeHardwareWatchpointControl()).
// There is no persistent "which watchpoint fired" status register on ARM64
// (unlike x86's DR6): the kernel's hw_breakpoint subsystem does that
// correlation for us and reports the exact faulting address via
// siginfo_t::si_addr on the SIGTRAP, so hit() matches that address against
// the enabled watchpoint ranges. This mirrors how lldb-server's own
// NativeRegisterContextLinux_arm64dbreg/NativeRegisterContextDBReg
// implementation determines the hit index from the other end of the wire.
//

ErrorCode HardwareBreakpointManager::enableLocation(Site const &site, int idx,
                                                    Target::Thread *thread) {
  uint64_t wvr;
  uint32_t wcr;
  CHK(ComputeARM64WatchpointControl(site.address, site.size, site.mode, wvr,
                                    wcr));

  Target::Process *process = thread->process();
  ProcessThreadId ptid(process->pid(), thread->tid());

  std::vector<uint64_t> addresses;
  std::vector<uint32_t> controls;
  CHK(process->ptrace().readHardwareWatchpointControl(ptid, addresses,
                                                       controls));

  if (static_cast<size_t>(idx) >= addresses.size()) {
    return kErrorInvalidArgument;
  }

  addresses[idx] = wvr;
  controls[idx] = wcr;

  return process->ptrace().writeHardwareWatchpointControl(ptid, addresses,
                                                           controls);
}

ErrorCode HardwareBreakpointManager::disableLocation(int idx,
                                                     Target::Thread *thread) {
  Target::Process *process = thread->process();
  ProcessThreadId ptid(process->pid(), thread->tid());

  std::vector<uint64_t> addresses;
  std::vector<uint32_t> controls;
  CHK(process->ptrace().readHardwareWatchpointControl(ptid, addresses,
                                                       controls));

  if (static_cast<size_t>(idx) >= addresses.size()) {
    return kErrorInvalidArgument;
  }

  addresses[idx] = 0;
  controls[idx] = 0;

  return process->ptrace().writeHardwareWatchpointControl(ptid, addresses,
                                                           controls);
}

int HardwareBreakpointManager::hit(Target::Thread *thread, Site &site) {
  if (_sites.empty()) {
    return -1;
  }

  if (thread->state() != Target::Thread::kStopped) {
    return -1;
  }

  Target::Process *process = thread->process();
  ProcessThreadId ptid(process->pid(), thread->tid());

  siginfo_t si;
  if (process->ptrace().getSigInfo(ptid, si) != kSuccess) {
    return -1;
  }

  if (si.si_code != TRAP_HWBKPT) {
    return -1;
  }

  uint64_t addr = reinterpret_cast<uint64_t>(si.si_addr);
  for (size_t i = 0; i < _locations.size(); ++i) {
    if (_locations[i] == 0) {
      continue;
    }

    auto it = _sites.find(_locations[i]);
    if (it == _sites.end()) {
      continue;
    }

    Site const &s = it->second;
    if (addr >= static_cast<uint64_t>(s.address) &&
        addr < static_cast<uint64_t>(s.address) + s.size) {
      site = s;
      return static_cast<int>(i);
    }
  }

  return -1;
}

} // namespace ds2
