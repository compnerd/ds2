//
// Copyright (c) 2014-present, Saleem Abdulrasool <compnerd@compnerd.org>
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Target/Process.h"

namespace ds2 {
namespace Target {
namespace Darwin {
ErrorCode Process::allocateMemory(size_t size, uint32_t protection,
                                  uint64_t *address) {
  if (address == nullptr || size == 0)
    return kErrorInvalidArgument;

  *address = 0;
  return kErrorUnsupported;
}

ErrorCode Process::deallocateMemory(uint64_t address, size_t size) {
  return kErrorUnsupported;
}
}
}
}
