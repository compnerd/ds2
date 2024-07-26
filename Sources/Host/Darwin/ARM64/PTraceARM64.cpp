//
// Copyright (c) 2024-present, Saleem Abdulrasool <compnerd@compnerd.org>
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Host/Darwin/PTrace.h"
#include "DebugServer2/Host/Platform.h"
#include "DebugServer2/Utils/Log.h"

#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/user.h>

#define super ds2::Host::POSIX::PTrace

namespace ds2 {
namespace Host {
namespace Darwin {
ErrorCode PTrace::readCPUState(const ProcessThreadId &ptid,
                               const ProcessInfo &pinfo,
                               Architecture::CPUState &state) {
  DS2BUG("impossible to use ptrace to %s on Darwin", __FUNCTION__);
}

ErrorCode PTrace::writeCPUState(const ProcessThreadId &ptid,
                                const ProcessInfo &pinfo,
                                const Architecture::CPUState &state) {
  DS2BUG("impossible to use ptrace to %s on Darwin", __FUNCTION__);
}
} // namespace Darwin
} // namespace Host
} // namespace ds2

