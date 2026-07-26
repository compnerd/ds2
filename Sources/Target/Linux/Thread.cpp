//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Target/Linux/Thread.h"
#include "DebugServer2/Core/SoftwareBreakpointManager.h"
#include "DebugServer2/Host/Linux/ExtraWrappers.h"
#include "DebugServer2/Host/Linux/PTrace.h"
#include "DebugServer2/Host/Linux/ProcFS.h"
#include "DebugServer2/Target/Process.h"
#include "DebugServer2/Utils/Log.h"
#include "DebugServer2/Utils/Stringify.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>

using ds2::Host::Linux::ProcFS;
using ds2::Utils::Stringify;

#define super ds2::Target::POSIX::Thread

namespace ds2 {
namespace Target {
namespace Linux {

Thread::Thread(Process *process, ThreadId tid) : super(process, tid) {}

ErrorCode Thread::updateStopInfo(int waitStatus) {
  super::updateStopInfo(waitStatus);

  switch (_stopInfo.event) {
  case StopInfo::kEventExit:
  case StopInfo::kEventKill:
    DS2ASSERT(_stopInfo.reason == StopInfo::kReasonNone);
    _state = kTerminated;
    return kSuccess;

  case StopInfo::kEventStop: {
    _state = kStopped;

    // These are the reasons why we might want to alter the stop info of a
    // thread:
    // (1) a thread traced with PTRACE_O_TRACECLONE calls clone(2), it (the
    //     caller of clone(2)) is stopped with a SIGTRAP, and control is given
    //     back to the tracer (us). The wait(2) status will then be constructed
    //     so that
    //       status >> 8 == (SIGTRAP | (PTRACE_EVENT_CLONE << 8))
    //     which results in WIFSTOPPED(status) == true and
    //     WSTOPSIG(status) == SIGTRAP. We mark the thread stopped for no
    //     reason so it just gets restarted immediately (see
    //     Linux::Process::wait);
    // (1b) a thread traced with PTRACE_O_TRACEFORK/TRACEVFORK calls
    //      fork(2)/vfork(2); reported the same way as (1), but via
    //      PTRACE_EVENT_FORK/VFORK instead of PTRACE_EVENT_CLONE, and
    //      reported as a real stop per the fork-events/vfork-events
    //      GDB-remote extension;
    // (1c) a thread that vfork(2)'d resumes after its child calls execve(2)
    //      or _exit(2) and stops sharing memory with it, reported via
    //      PTRACE_EVENT_VFORK_DONE the same way as (1)/(1b);
    // (2) we sent the thread a SIGSTOP (with tkill(2)) to suspend it e.g.:
    //     when a thread hits a breakpoint, we have to stop every other thread,
    //     so we send each one of them a SIGSTOP with tkill(2). These other
    //     treads will be marked as stopped for no reason so the debugger can
    //     adapt its output (e.g.: lldb will simply hide these threads and only
    //     display the one that stopped for a breakpoint);
    // (3) we sent the process a SIGSTOP (with kill(2)) to interrupt it
    //     entirely. This happens when the user hits Ctrl-C and the debugger
    //     sends us a "\x03" for instance;
    // (4) the inferior received a SIGSTOP because of ptrace attach. We have to
    //     mark the thread as stopped for a trap;
    // (5) the inferior received a SIGTRAP. This is usually because of a
    //     breakpoint, single step or such;

    siginfo_t si;
    ProcessThreadId ptid(process()->pid(), tid());
    ErrorCode error = process()->ptrace().getSigInfo(ptid, si);
    if (error != kSuccess) {
      DS2LOG(Warning, "unable to get siginfo_t for tid %d, errno=%s", tid(),
             Stringify::Errno(errno));
      return error;
    }

    static constexpr int kEventClone = SIGTRAP | (PTRACE_EVENT_CLONE << 8);
    static constexpr int kEventFork = SIGTRAP | (PTRACE_EVENT_FORK << 8);
    static constexpr int kEventVFork = SIGTRAP | (PTRACE_EVENT_VFORK << 8);
    static constexpr int kEventVForkDone =
      SIGTRAP | (PTRACE_EVENT_VFORK_DONE << 8);
    const int waitStatusHi = waitStatus >> 8;

    if (waitStatusHi == kEventClone) { // (1)
      _stopInfo.event = StopInfo::kEventNone;
      _stopInfo.reason = StopInfo::kReasonThreadSpawn;
    } else if (waitStatusHi == kEventFork || waitStatusHi == kEventVFork) { // (1b)
      bool isVFork = waitStatusHi == kEventVFork;
      unsigned long childPid = 0;
      ErrorCode eventError =
          process()->ptrace().getEventMessage(ptid, childPid);
      if (eventError != kSuccess) {
        DS2LOG(Warning,
               "unable to get %s child pid for tid %d, errno=%s",
               isVFork ? "vfork" : "fork", tid(), Stringify::Errno(errno));
      }

      ErrorCode detachError = kSuccess;
      if (eventError == kSuccess) {
        int childStatus = 0;
        pid_t waited = ::waitpid(static_cast<pid_t>(childPid), &childStatus,
                                 __WALL);
        if (waited != static_cast<pid_t>(childPid)) {
          DS2LOG(Warning,
                 "unable to collect initial stop for %s child pid %lu "
                 "(tid %d), errno=%s",
                 isVFork ? "vfork" : "fork", childPid, tid(),
                 Stringify::Errno(errno));
          detachError = kErrorProcessNotFound;
        } else {
          detachError = process()->ptrace().detach(static_cast<ProcessId>(childPid));
          if (detachError != kSuccess && detachError != kErrorProcessNotFound) {
            DS2LOG(Warning,
                   "unable to detach %s child pid %lu (tid %d), error=%d",
                   isVFork ? "vfork" : "fork", childPid, tid(), detachError);
          }
        }
      }

      if (eventError == kSuccess &&
          (detachError == kSuccess || detachError == kErrorProcessNotFound)) {
        _stopInfo.reason =
            isVFork ? StopInfo::kReasonVFork : StopInfo::kReasonFork;
        _stopInfo.child = ProcessThreadId(static_cast<ProcessId>(childPid),
                                          static_cast<ThreadId>(childPid));
      }
    } else if (waitStatusHi == kEventVForkDone) { // (1c)
      _stopInfo.reason = StopInfo::kReasonVForkDone;

      unsigned long childPid = 0;
      ErrorCode eventError =
          process()->ptrace().getEventMessage(ptid, childPid);
      if (eventError == kSuccess) {
        _stopInfo.child = ProcessThreadId(static_cast<ProcessId>(childPid),
                                          static_cast<ThreadId>(childPid));
      }
    } else if (si.si_code == SI_TKILL && si.si_pid == getpid()) { // (2)
      // The only signal we are supposed to send to the inferior is a SIGSTOP.
      DS2ASSERT(_stopInfo.signal == SIGSTOP);
      _stopInfo.event = StopInfo::kEventNone;
    } else if (si.si_code == SI_USER && si.si_pid == getpid()) { // (3)
      DS2ASSERT(_stopInfo.signal == SIGSTOP);
      _stopInfo.reason = StopInfo::kReasonSignalStop;
    } else if (si.si_code == SI_USER && si.si_pid == 0 &&
               _stopInfo.signal == SIGSTOP) { // (4)
      _stopInfo.reason = StopInfo::kReasonTrap;
    } else if (_stopInfo.signal == SIGTRAP) { // (5)
      switch (si.si_code) {
      case TRAP_HWBKPT:
      case TRAP_TRACE: {
        auto *hwBpm = process()->hardwareBreakpointManager();
        if (hwBpm == nullptr || !hwBpm->fillStopInfo(this, _stopInfo)) {
          _stopInfo.reason = StopInfo::kReasonTrace;
        }
        break;
      }
      case SI_KERNEL:
      case TRAP_BRKPT:
        _stopInfo.reason = StopInfo::kReasonBreakpoint;
        break;
      default:
        if (si.si_code > 0 || si.si_pid == getpid())
          DS2BUG("unknown sigtrap code %d", si.si_code);

        // If the value of si_code is <= 0, then the signal was generated by a
        // user process via kill(2), sigsend(2), abort(3C), raise(3C), etc. No
        // action is needed other than updating the thread's stop info.
        DS2LOG(Debug, "sigtrap code %d recieved from %" PRI_PID, si.si_code,
               si.si_pid);
        _stopInfo.reason = StopInfo::kReasonTrap;
        break;
      }
    } else {
      // This is not a signal that we originated. We can output a
      // warning if the signal comes from an external source.
      _stopInfo.reason = StopInfo::kReasonSignalStop;
      if ((si.si_code == SI_USER || si.si_code == SI_TKILL) &&
          si.si_pid != tid())
        DS2LOG(Warning,
               "tid %d received signal %s from an external source (sender=%d)",
               tid(), strsignal(_stopInfo.signal), si.si_pid);

      // For kernel-generated memory-fault signals, si_addr carries the
      // faulting address. User-generated SIGSEGV/SIGBUS reports have
      // si_code <= 0 and a different active union member.
      if ((_stopInfo.signal == SIGSEGV || _stopInfo.signal == SIGBUS) &&
          si.si_code > 0) {
        _stopInfo.fault = reinterpret_cast<uint64_t>(si.si_addr);
      }
    }
  } break;

  default:
    DS2BUG("impossible StopInfo event: %s",
           Stringify::StopEvent(_stopInfo.event));
  }

  return kSuccess;
}

void Thread::updateState() {
  if (!process()->isAlive()) {
    _state = kTerminated;
    return;
  }

  ProcFS::Stat stat;
  if (!ProcFS::ReadStat(_process->pid(), tid(), stat)) {
    stat.task_cpu = 0;
    stat.state = 0;
  }

  _stopInfo.core = stat.task_cpu;

  State oldState = _state;

  switch (stat.state) {
  case Host::Linux::kProcStateZombie:
  case Host::Linux::kProcStateDead:
    _state = kTerminated;
    break;

  case Host::Linux::kProcStateUninterruptible:
  case Host::Linux::kProcStateSleeping:
  case Host::Linux::kProcStateRunning:
  case Host::Linux::kProcStatePaging:
    _state = kRunning;
    break;

  case Host::Linux::kProcStateTraced:
  case Host::Linux::kProcStateStopped:
    _state = kStopped;
    break;
  default:
    _state = kInvalid;
    break;
  }

  // If the thread state has gone from running to a non-running state,
  // update the stop info, to maintain consistentency between
  // the state and the stop info.
  if (oldState == kRunning && _state != kRunning) {
    int status;
    int ret = ::waitpid(tid(), &status, __WALL | WNOHANG);
    DS2ASSERT(ret >= 0);

    updateStopInfo(status);
  }
}
} // namespace Linux
} // namespace Target
} // namespace ds2
