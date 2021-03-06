//
// Copyright (c) 2014, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Host/POSIX/PTrace.h"

#include <sys/wait.h>
#include <cerrno>

using ds2::Host::POSIX::PTrace;
using ds2::ErrorCode;

PTrace::PTrace() {}

PTrace::~PTrace() {}

ErrorCode PTrace::wait(ProcessThreadId const &ptid, bool hang, int *status) {
  if (ptid.pid <= kAnyProcessId || !(ptid.tid <= kAnyThreadId))
    return kErrorInvalidArgument;

  int stat;
  pid_t wpid = ::waitpid(ptid.pid, &stat, hang ? 0 : WNOHANG);
  if (wpid != ptid.pid) {
    switch (errno) {
    case ESRCH:
      return kErrorProcessNotFound;
    default:
      return kErrorInvalidArgument;
    }
  }

  if (status != nullptr) {
    *status = stat;
  }

  return kSuccess;
}

//
// Execute will execute the code in the target process/thread,
// the techinique for PTRACE is to read CPU state, rewrite the
// code portion pointed by PC with the code, resuming the process/thread,
// wait for the completion, read the CPU state back to grab return
// values, then restoring the previous code.
//
ErrorCode PTrace::execute(ProcessThreadId const &ptid, ProcessInfo const &pinfo,
                          void const *code, size_t length, uint64_t &result) {
  Architecture::CPUState savedState, resultState;
  std::string savedCode;

  if (!ptid.valid() || code == nullptr || length == 0)
    return kErrorInvalidArgument;

  // 1. Read and save the CPU state
  ErrorCode error = readCPUState(ptid, pinfo, savedState);
  if (error != kSuccess)
    return error;

  // 2. Copy the code at PC
  savedCode.resize(length);
  error = readMemory(ptid, savedState.pc(), &savedCode[0], length);
  if (error != kSuccess)
    return error;

  // 3. Write the code to execute at PC
  error = writeMemory(ptid, savedState.pc(), code, length);
  if (error != kSuccess)
    goto fail;

  // 3. Resume and wait
  error = resume(ptid, pinfo);
  if (error == kSuccess) {
    error = wait(ptid);
  }

  if (error == kSuccess) {
    // 4. Read back the CPU state
    error = readCPUState(ptid, pinfo, resultState);
    if (error == kSuccess) {
      // 5. Save the result
      result = resultState.retval();
    }
  }

  // 6. Write back the old code
  error = writeMemory(ptid, savedState.pc(), &savedCode[0], length);
  if (error != kSuccess)
    goto fail;

  // 7. Restore CPU state
  error = writeCPUState(ptid, pinfo, savedState);
  if (error != kSuccess)
    goto fail;

  // Success!! We injected and executed code!
  return kSuccess;

fail:
  return kill(ptid, SIGKILL); // we can't really do much at this point :(
}

ErrorCode PTrace::TranslateErrno(int error) {
  switch (error) {
  case EBUSY:
    return ds2::kErrorBusy;
  case ESRCH:
    return ds2::kErrorProcessNotFound;
  case EFAULT:
    return ds2::kErrorInvalidAddress;
  case EIO:
    return ds2::kErrorInvalidAddress;
  case EPERM:
    return ds2::kErrorNoPermission;
  default:
    break;
  }
  return ds2::kErrorInvalidArgument;
}

ErrorCode PTrace::TranslateErrno() { return TranslateErrno(errno); }
