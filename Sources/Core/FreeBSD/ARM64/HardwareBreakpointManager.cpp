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

namespace ds2 {

// Real ARM64 hardware watchpoint support has only been implemented for
// Linux (Sources/Core/Linux/ARM64/HardwareBreakpointManager.cpp) and Windows
// (Sources/Core/Windows/ARM64/HardwareBreakpointManager.cpp) so far; FreeBSD
// keeps the previous stub behavior. getMaxWatchpoints() is unimplemented for
// this host too, so maxWatchpoints() (and therefore add()) already refuses
// to enable any watchpoint before these would ever be reached.

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

} // namespace ds2
