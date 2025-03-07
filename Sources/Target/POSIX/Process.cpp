//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Target/Process.h"
#include "DebugServer2/Core/BreakpointManager.h"
#include "DebugServer2/Host/Platform.h"
#include "DebugServer2/Host/POSIX/PTrace.h"
#include "DebugServer2/Target/Thread.h"
#include "DebugServer2/Utils/Log.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <memory>

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

using ds2::Host::ProcessSpawner;

#define super ds2::Target::ProcessBase

namespace ds2 {
namespace Target {
namespace POSIX {

ErrorCode Process::initialize(ProcessId pid, uint32_t flags) {
  // Wait the main thread.
  int status;
  CHK(ptrace().wait(pid, &status));
  CHK(ptrace().traceThat(pid));

  // Can't use `CHK()` here because of a bug in GCC. See
  // http://stackoverflow.com/a/12086873/5731788
  ErrorCode error = super::initialize(pid, flags);
  if (error != kSuccess) {
    return error;
  }

  CHK(attach(status));

  return kSuccess;
}

ErrorCode Process::detach() {
  prepareForDetach();

  CHK(ptrace().detach(_pid));

  cleanup();
  _flags &= ~kFlagAttachedProcess;

  return kSuccess;
}

ErrorCode Process::interrupt() { return ptrace().kill(_pid, SIGSTOP); }

ErrorCode Process::terminate() {
  // We have to use SIGKILL here, for two (related) reasons:
  // (1) we want to make sure the tracee can't catch the signal when we are
  //     trying to terminate it;
  // (2) using SIGKILL allows us to send the signal with a simple `kill()`
  //     instead of having to use ptrace(PTRACE_CONT, ...)`.
  return ptrace().kill(_pid, SIGKILL);
}

bool Process::isAlive() const { return (_pid > 0 && ::kill(_pid, 0) == 0); }

ErrorCode Process::readString(Address const &address, std::string &str,
                              size_t length, size_t *count) {
  auto id = _currentThread == nullptr ? _pid : _currentThread->tid();
  return ptrace().readString(id, address, str, length, count);
}

ErrorCode Process::readMemory(Address const &address, void *data, size_t length,
                              size_t *count) {
  auto id = _currentThread == nullptr ? _pid : _currentThread->tid();
  return ptrace().readMemory(id, address, data, length, count);
}

ErrorCode Process::writeMemory(Address const &address, void const *data,
                               size_t length, size_t *count) {
  auto id = _currentThread == nullptr ? _pid : _currentThread->tid();
  return ptrace().writeMemory(id, address, data, length, count);
}

int Process::convertMemoryProtectionToPOSIX(uint32_t protection) const {
  int res = PROT_NONE;

  if (protection & kProtectionRead) {
    res |= PROT_READ;
  }
  if (protection & kProtectionWrite) {
    res |= PROT_WRITE;
  }
  if (protection & kProtectionExecute) {
    res |= PROT_EXEC;
  }

  return res;
}

uint32_t Process::convertMemoryProtectionFromPOSIX(int POSIXProtection) const {
  uint32_t res = kProtectionNone;

  if (POSIXProtection & PROT_READ) {
    res |= kProtectionRead;
  }
  if (POSIXProtection & PROT_WRITE) {
    res |= kProtectionWrite;
  }
  if (POSIXProtection & PROT_EXEC) {
    res |= kProtectionExecute;
  }

  return res;
}

ErrorCode Process::wait() { return ptrace().wait(_pid, nullptr); }

ds2::Target::Process *Process::Attach(ProcessId pid) {
  if (pid <= 0)
    return nullptr;

  auto process = std::make_unique<Target::Process>();

  if (process->ptrace().attach(pid) != kSuccess) {
    DS2LOG(Error, "ptrace attach failed: %s", strerror(errno));
    return nullptr;
  }

  if (process->initialize(pid, kFlagAttachedProcess) != kSuccess) {
    process->ptrace().detach(pid);
    return nullptr;
  }

  return process.release();
}

ds2::Target::Process *Process::Create(ProcessSpawner &spawner) {
  auto process = std::make_unique<Target::Process>();

  if (spawner.run([&process]() {
        // move debug targets to their own process group
        if (::setpgid(0, 0) != 0)
          return false;

        // drop the setgid ability of debug targets
        if (::setgid(::getgid()) != 0)
          return false;

        return process->ptrace().traceMe(true) == kSuccess;
      }) != kSuccess) {
    return nullptr;
  }

  pid_t pid = spawner.pid();
  DS2LOG(Debug, "created process %d", pid);

  if (process->initialize(pid, kFlagNewProcess) != kSuccess) {
    return nullptr;
  }

  // Give a chance to the ptrace implementation to set any specific options.
  if (process->ptrace().traceThat(pid) != kSuccess) {
    return nullptr;
  }

  return process.release();
}

void Process::resetSignalPass() { _passthruSignals.clear(); }

void Process::setSignalPass(int signo, bool set) {
  if (set) {
    _passthruSignals.insert(signo);
  } else {
    _passthruSignals.erase(signo);
  }
}

// Used to interrupt calls to wait(2), waitpid(2), and waitid(2) that are
// blocked waiting for inferior process events. Requried in the scenario where
// the debug server's main thread is blocked waiting for events from an
// inferior process whose threads are already in a stopped state. E.g.:
// 1. All threads of the inferior process are in a stopped state.
// 2. The main thread of the debug server is blocked in a waitpid() call
//    waiting for an event from the inferior process.
// 2. The debug server process receives an asynchronous \x03 interrupt packet
//    from the debugger and attempts to interrupt the inferior by calling
//    kill(SIGSTOP).
// 3. The kill(SIGSTOP) call has no effect on the inferior process because
//    every thread is already stopped.
// 4. The call to waitpid() remains blocked indefinitely.
//
// In this situation, the debug server can use sendInterrupt in the following
// way to interrupt the waitpid() call:
// 1. In response to the interrupt packet from the debugger, the debug server
//    calls sendInterrupt() in addition to kill(SIGSTOP). This call forks a
//    short-lived child process and saves its pid. The child process
//    immediately exits, generating a thread exit event.
// 2. The debug server's main thread now returns from waitpid(). It calls
//    checkInterrupt() with the results from waitpid(). The call compares the
//    pid returned by waitpid() with the pid stashed by sendInterrupt(), which
//    returns true if pid matches the one saved during the last call to
//    sendInterrupt().
//
// NOTE: There can be only one pending interrupt at a time. Subsequent calls
// to sendInterrupt() without corresponding calls to checkInterrupt() are
// noops.
ErrorCode Process::sendInterrupt() {
  // Mutex guards against concurrent calls to checkInterrupt and sendInterrupt.
  // It is required to atomically fork the interrupting process and memoize it's
  // pid in the _pid field.
  std::lock_guard<std::mutex> lock(_interruptState.mutex);
  if (_interruptState.pid > 0) // There is already a pending interrupt
    return kSuccess;

  const ProcessId pid = ::fork();
  if (pid < 0)
    return Host::Platform::TranslateError();

  if (pid == 0) {
    DS2LOG(Debug, "forked process %" PRI_PID " to interrupt waiter",
           ::getpid());
    // Exiting the forked thread will wake the waiting parent process with
    // WIFEXITED status.
    exit(0);
  }

  _interruptState.pid = pid;
  return kSuccess;
}

bool Process::checkInterrupt(ThreadId tid, int waitStatus) {
  {
    std::lock_guard<std::mutex> lock(_interruptState.mutex);
    if (_interruptState.pid <= 0) // no interrupt
      return false;

    if (_interruptState.pid != tid || !WIFEXITED(waitStatus)) // tid does not match interrupt
      return false;

    _interruptState.pid = 0; // clear interrupt
  }

  DS2LOG(Debug, "received interrupt from process %" PRI_PID, tid);
  return true;
}
} // namespace POSIX
} // namespace Target
} // namespace ds2
