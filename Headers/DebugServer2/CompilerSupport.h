//
// Copyright (c) 2014, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#ifndef __DebugServer2_CompilerSupport_h
#define __DebugServer2_CompilerSupport_h

#if !defined(__has_attribute)
#define __has_attribute(ATTR) 0
#endif

#if !defined(__GNUC_PREREQ)
#define __GNUC_PREREQ(MAJOR, MINOR) 0
#endif

#if __has_attribute(format) || __GNUC_PREREQ(3, 4)
#define DS2_ATTRIBUTE_PRINTF(FORMAT, START) \
    __attribute__((__format__(printf, FORMAT, START)))
#else
#define DS2_ATTRIBUTE_PRINTF(FORMAT, START)
#endif


#endif // !__DebugServer2_CompilerSupport_h
