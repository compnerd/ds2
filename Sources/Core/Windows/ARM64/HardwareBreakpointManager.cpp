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
#include "DebugServer2/Target/Process.h"
#include "DebugServer2/Target/Windows/Thread.h"
#include "DebugServer2/Utils/Log.h"

namespace ds2 {

//
// Windows: hardware watchpoints are exposed via Wcr[]/Wvr[] in
// CONTEXT_DEBUG_REGISTERS, read/written through
// Target::Windows::Thread::{read,write}WatchpointRegisters().
//
// Unlike x86 (which has DR6, a persistent "which register fired" status
// register), the ARM64 ARM_NT_CONTEXT exposes no equivalent for
// watchpoints, and (as far as could be determined without access to real
// ARM64 hardware/kernel documentation) neither does the EXCEPTION_RECORD
// delivered for the resulting STATUS_SINGLE_STEP. This is a known
// limitation: when exactly one watchpoint is armed, hit() can unambiguously
// report it; with more than one simultaneously-armed watchpoint, there is no
// documented way found here to determine which slot actually trapped, and
// this code falls back to reporting the last enabled slot with a logged
// warning. This should be revisited against real Windows ARM64 hardware
// (notably, whether EXCEPTION_RECORD::ExceptionInformation carries a fault
// address for this exception the way it does for STATUS_ACCESS_VIOLATION).
//
// Sources/Target/Windows/Thread.cpp calls fillStopInfo() (and therefore
// hit()) for both STATUS_BREAKPOINT and STATUS_SINGLE_STEP, so hit() must
// rule out stops that cannot possibly be this watchpoint firing before
// picking a slot:
//
//   - STATUS_BREAKPOINT is a software breakpoint (a BRK instruction trap);
//     ARM64 watchpoints only ever deliver as STATUS_SINGLE_STEP, so this
//     can never be a watchpoint.
//   - STATUS_SINGLE_STEP for a stop Thread::step() itself requested (see
//     Thread::wasExplicitStep()) is ambiguous rather than ruled out: the
//     stepped instruction may or may not have also touched a watched byte,
//     and Windows gives no way to tell from the exception alone (PSTATE.SS
//     cannot be used either: the architectural software-step exception
//     transition clears it before the stopped context can be read,
//     regardless of cause). _lastKnownValues resolves this by comparing
//     each enabled watchpoint's memory against what it held when last
//     checked; whichever one changed is the hit, and if none did, this was
//     an ordinary step. This can only ever confirm a write/access
//     watchpoint this way, not a pure read watchpoint (reading doesn't
//     change the value), which is a known gap in this specific ambiguous
//     case.
//

ErrorCode HardwareBreakpointManager::enableLocation(Site const &site, int idx,
                                                    Target::Thread *thread) {
  uint64_t wvr;
  uint32_t wcr;
  CHK(ComputeARM64WatchpointControl(site.address, site.size, site.mode, wvr,
                                    wcr));

  std::vector<uint64_t> addresses;
  std::vector<uint32_t> controls;
  CHK(thread->readWatchpointRegisters(addresses, controls));

  if (static_cast<size_t>(idx) >= addresses.size()) {
    return kErrorInvalidArgument;
  }

  addresses[idx] = wvr;
  controls[idx] = wcr;

  CHK(thread->writeWatchpointRegisters(addresses, controls));

  // Seed the value cache hit() uses to disambiguate an explicit step from
  // a watchpoint hit (see the file comment above). Best-effort: if the
  // range can't be read right now (e.g. not yet mapped), just leave this
  // slot without a cached value rather than an outdated one; hit() skips
  // slots it has no cached value for.
  ByteVector value;
  if (thread->process()->readMemoryBuffer(site.address, site.size, value) ==
      kSuccess) {
    _lastKnownValues[idx] = std::move(value);
  } else {
    _lastKnownValues.erase(idx);
  }

  return kSuccess;
}

ErrorCode HardwareBreakpointManager::disableLocation(int idx,
                                                     Target::Thread *thread) {
  std::vector<uint64_t> addresses;
  std::vector<uint32_t> controls;
  CHK(thread->readWatchpointRegisters(addresses, controls));

  if (static_cast<size_t>(idx) >= addresses.size()) {
    return kErrorInvalidArgument;
  }

  addresses[idx] = 0;
  controls[idx] = 0;

  CHK(thread->writeWatchpointRegisters(addresses, controls));
  _lastKnownValues.erase(idx);
  return kSuccess;
}

int HardwareBreakpointManager::hit(Target::Thread *thread, Site &site) {
  if (_sites.empty()) {
    return -1;
  }

  if (thread->state() != Target::Thread::kStopped) {
    return -1;
  }

  auto *winThread = static_cast<Target::Windows::Thread *>(thread);
  bool ambiguousStep = false;
  switch (winThread->lastExceptionCode()) {
  case STATUS_BREAKPOINT:
    // Never a watchpoint; see the file comment above.
    return -1;

  case STATUS_SINGLE_STEP:
    ambiguousStep = winThread->wasExplicitStep();
    break;

  default:
    break;
  }

  if (ambiguousStep) {
    // Thread::step() itself requested this stop; find whichever enabled
    // watchpoint's memory actually changed, if any (see the file comment
    // above).
    for (size_t i = 0; i < _locations.size(); ++i) {
      if (_locations[i] == 0) {
        continue;
      }

      auto siteIt = _sites.find(_locations[i]);
      if (siteIt == _sites.end()) {
        continue;
      }

      auto cacheIt = _lastKnownValues.find(static_cast<int>(i));
      if (cacheIt == _lastKnownValues.end()) {
        continue;
      }

      ByteVector current;
      if (thread->process()->readMemoryBuffer(
              siteIt->second.address, siteIt->second.size, current) !=
          kSuccess) {
        continue;
      }

      if (current != cacheIt->second) {
        cacheIt->second = std::move(current);
        site = siteIt->second;
        return static_cast<int>(i);
      }
    }

    // No enabled watchpoint's memory changed: the ordinary completion of
    // the requested step, not a watchpoint.
    return -1;
  }

  int hitIdx = -1;
  int enabledCount = 0;
  for (size_t i = 0; i < _locations.size(); ++i) {
    if (_locations[i] == 0) {
      continue;
    }

    auto it = _sites.find(_locations[i]);
    if (it == _sites.end()) {
      continue;
    }

    ++enabledCount;
    hitIdx = static_cast<int>(i);
  }

  if (hitIdx < 0) {
    return -1;
  }

  if (enabledCount > 1) {
    DS2LOG(Warning,
           "%d hardware watchpoints are armed; Windows ARM64 cannot "
           "determine which one actually trapped, reporting slot %d",
           enabledCount, hitIdx);
  }

  site = _sites.find(_locations[hitIdx])->second;

  // Refresh the cached value for the reported slot so a later explicit-step
  // comparison (see above) starts from an accurate baseline.
  ByteVector current;
  if (thread->process()->readMemoryBuffer(site.address, site.size, current) ==
      kSuccess) {
    _lastKnownValues[hitIdx] = std::move(current);
  }

  return hitIdx;
}

} // namespace ds2
