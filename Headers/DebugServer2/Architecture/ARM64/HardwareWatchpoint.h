//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#pragma once

#include "DebugServer2/Core/BreakpointManager.h"
#include "DebugServer2/Core/ErrorCodes.h"
#include "DebugServer2/Types.h"

namespace ds2 {

//
// DBGWVRn_EL1/DBGWCRn_EL1 (or Wvr[]/Wcr[] on Windows) bit layout, shared by
// every OS-specific HardwareBreakpointManager implementation for ARM64. The
// encoding mirrors LLDB's own client-side implementation for the same
// hardware (NativeRegisterContextDBReg_arm64.cpp /
// NativeRegisterContextLinux_arm64dbreg.cpp), which is the most authoritative
// available cross-check for the exact encoding short of the ARMv8-A
// Architecture Reference Manual itself:
//
//   bit    0    : E    (enable)
//   bits 2:1    : PAC  (privilege access control; 0b10, EL0-only, is what
//                       LLDB programs and is what we mirror here)
//   bits 4:3    : LSC  (load/store control: 01 for load/read, 10 for
//                       store/write, 11 for either)
//   bits 12:5   : BAS  (byte address select, one bit per watched byte
//                       within the 8-byte-aligned doubleword pointed at by
//                       the value register)
//

constexpr uint32_t kARM64WatchpointEnable = 1u << 0;
constexpr uint32_t kARM64WatchpointPAC = 0x2u << 1;
constexpr uint32_t kARM64WatchpointLSCShift = 3;
constexpr uint32_t kARM64WatchpointBASShift = 5;

// Computes the DBGWVRn_EL1/DBGWCRn_EL1 (or Wvr[]/Wcr[] on Windows) pair that
// implements a watchpoint over [address, address + size). `address`/`size`
// are validated (size in {1, 2, 4, 8}, and the range fits within a single
// 8-byte watchpoint granule) by HardwareBreakpointManager::isValid() before
// this is ever reached.
inline ErrorCode ComputeARM64WatchpointControl(Address const &address,
                                               size_t size,
                                               BreakpointManager::Mode mode,
                                               uint64_t &wvr, uint32_t &wcr) {
  uint32_t lsc;
  switch (static_cast<int>(mode)) {
  case BreakpointManager::kModeWrite:
    lsc = 0x2;
    break;
  case BreakpointManager::kModeRead:
    lsc = 0x1;
    break;
  case BreakpointManager::kModeRead | BreakpointManager::kModeWrite:
    lsc = 0x3;
    break;
  default:
    return kErrorInvalidArgument;
  }

  uint64_t addr = address;
  unsigned offset = static_cast<unsigned>(addr & 0x7);
  unsigned span = offset + static_cast<unsigned>(size);
  if (span > 8) {
    // Does not fit in a single hardware watchpoint slot.
    return kErrorInvalidArgument;
  }

  // BAS only needs to be a contiguous run of bits within the 8-byte
  // granule; the hardware does not require that run to itself be aligned
  // to its own size (this matches the Linux kernel's own arm64
  // hw_breakpoint validation, which simply shifts the size-based mask by
  // the raw offset). So the exact requested range can always be expressed
  // directly: e.g. a 2-byte watchpoint at the granule's byte 1 is BAS 0x06
  // (bytes 1-2), not a widened 4-byte range starting at byte 0.
  uint32_t bas = static_cast<uint32_t>((1u << size) - 1) << offset;

  wvr = addr & ~static_cast<uint64_t>(0x7);
  wcr = kARM64WatchpointEnable | kARM64WatchpointPAC |
        (lsc << kARM64WatchpointLSCShift) | (bas << kARM64WatchpointBASShift);
  return kSuccess;
}

} // namespace ds2
