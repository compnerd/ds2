//
// Copyright (c) 2024, Saleem Abdulrasool <compnerd@compnerd.org>
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Target/Darwin/Thread.h"
#include "DebugServer2/Architecture/CPUState.h"

namespace ds2 {
namespace Target {
namespace Darwin {
ErrorCode Thread::step(int signal, Address const &address) {
  return kErrorUnsupported;
}

ErrorCode Thread::afterResume() {
  return kErrorUnsupported;
}
}
}
}
