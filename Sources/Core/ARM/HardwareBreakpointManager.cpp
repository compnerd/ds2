//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Core/HardwareBreakpointManager.h"
#include "DebugServer2/Target/Process.h"
#include "DebugServer2/Utils/Log.h"

#include <algorithm>

#define super ds2::BreakpointManager

namespace ds2 {

#if defined(ARCH_ARM64)

//
// This is a real implementation of ARM64 hardware watchpoints (data
// breakpoints); the OS-specific register access and hit-detection logic
// lives alongside the rest of each OS's target support, following the same
// Host/<OS>/<Arch> and Target/<OS>/<Arch> convention used elsewhere in the
// tree:
//
//   - Sources/Core/Linux/ARM64/HardwareBreakpointManager.cpp
//   - Sources/Core/Windows/ARM64/HardwareBreakpointManager.cpp
//   - Sources/Core/Darwin/ARM64/HardwareBreakpointManager.cpp (stub)
//   - Sources/Core/FreeBSD/ARM64/HardwareBreakpointManager.cpp (stub)
//
// This file only holds the parts that do not depend on the host OS: size/mode
// validation and the shared bit-layout helper in
// Headers/DebugServer2/Architecture/ARM64/HardwareWatchpoint.h. Hardware
// *execute* breakpoints (DBGBVR/DBGBCR, accessed via NT_ARM_HW_BREAK on Linux
// or Bcr/Bvr on Windows) are architecturally a completely separate register
// file from watchpoints (DBGWVR/DBGWCR, NT_ARM_HW_WATCH / Wcr/Wvr) and are out
// of scope here; ARM64 execute breakpoints continue to go through
// SoftwareBreakpointManager, same as before this change.
//

size_t HardwareBreakpointManager::chooseBreakpointSize(Address const &) const {
  // Hardware watchpoints are always explicitly sized by the client (the
  // GDB-remote Z2/Z3/Z4 packets always carry an explicit length), so this
  // should not normally be reached in practice. Unlike the x86
  // implementation, return a sensible default instead of asserting.
  return sizeof(uint64_t);
}

ErrorCode HardwareBreakpointManager::isValid(Address const &address,
                                             size_t size, Mode mode) const {
  switch (size) {
  case 1:
  case 2:
  case 4:
  case 8:
    break;
  default:
    DS2LOG(Debug, "unsupported hardware watchpoint size %zu", size);
    return kErrorInvalidArgument;
  }

  if (mode == kModeExec) {
    // Hardware execute breakpoints live in a completely different register
    // file (DBGBVR/DBGBCR) from data watchpoints (DBGWVR/DBGWCR), which is
    // all this class programs; software breakpoints handle exec.
    DS2LOG(Debug,
           "hardware execute breakpoints are not supported through the ARM64 "
           "watchpoint interface");
    return kErrorUnsupported;
  }

  if ((mode & kModeExec) && (mode & (kModeRead | kModeWrite))) {
    DS2LOG(Debug, "trying to set a hardware watchpoint with mixed exec and "
                  "read/write modes");
    return kErrorInvalidArgument;
  }

  uint64_t addr = address;
  if ((addr & 0x7) + size > 8) {
    DS2LOG(Debug,
           "hardware watchpoint at %" PRI_PTR " of size %zu straddles more "
           "than a single 8-byte watchpoint granule",
           PRI_PTR_CAST(addr), size);
    return kErrorInvalidArgument;
  }

  return super::isValid(address, size, mode);
}

#else // !ARCH_ARM64 (32-bit ARM)

int HardwareBreakpointManager::hit(Target::Thread *thread, Site &site) {
  return -1;
}

ErrorCode HardwareBreakpointManager::enableLocation(Site const &site, int idx,
                                                    Target::Thread *thread) {
  return kErrorUnsupported;
}

ErrorCode HardwareBreakpointManager::disableLocation(int idx,
                                                     Target::Thread *thread) {
  return kErrorUnsupported;
}

ErrorCode HardwareBreakpointManager::isValid(Address const &address,
                                             size_t size, Mode mode) const {
  DS2LOG(Debug, "Trying to set hardware breakpoint on arm");
  return kErrorUnsupported;
}

size_t HardwareBreakpointManager::chooseBreakpointSize(Address const &) const {
  DS2BUG(
      "Choosing a hardware breakpoint size on ARM is an unsupported operation");
}

#endif // ARCH_ARM64

size_t HardwareBreakpointManager::maxWatchpoints() {
  return _process->getMaxWatchpoints();
}
} // namespace ds2
