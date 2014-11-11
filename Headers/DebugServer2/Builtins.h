//
// Copyright (c) 2014, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#ifndef __DebugServer2_Builtins_h
#define __DebugServer2_Builtins_h

#include "DebugServer2/CompilerSupport.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if !__has_builtin(__builtin_ffs)
#if defined(_MSC_VER)
static inline unsigned long __builtin_ffs(unsigned long mask) {
  unsigned long index;
  _BitScanForward(&index, mask);
  return index;
}
#endif
#endif

#if !__has_builtin(__builtin_popcount)
#if defined(_MSC_VER)
static inline unsigned int __builtin_popcount(unsigned int value) {
  return __popcnt(value);
}
#endif
#endif

#endif

