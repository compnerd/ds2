#pragma once

#include "DebugServer2/Types.h"

#include <mutex>

namespace ds2 {
namespace Host {
namespace POSIX {

// WaitInterrupter is used to interrupt calls to wait(2), waitpid(2), and
// waitid(2) that are blocked waiting for inferior process events.
//
// It is useful in the scenario where the debug server's main thread is blocked
// waiting for events from an inferior process whose threads are already in a
// stopped state. E.g.:
//
// 1. All threads of the inferior process are in a stopped state.
// 2. The main thread of the debug server is blocked in a waitpid() call waiting
//    for an event from the inferior process.
// 2. The debug server process receives an asynchronous \x03 interrupt packet
//    from the debugger and attempts to interrupt the inferior by calling
//    kill(SIGSTOP).
// 3. The kill(SIGSTOP) call has no effect on the inferior process because every
//    thread is already stopped.
// 4. The call to waitpid() remains blocked indefinitely.
//
// In this situation, the debug server can use WaitInterrupter in the following
// way to interrupt the waitpid() call:
//
// 1. In response to the interrupt packet from the debugger, the debug server
//    calls sendInterrupt() in addition to kill(SIGSTOP). This call forks a
//    short-lived child process and saves its pid. The child process immediately
//    exits, generating a thread exit event.
// 2. The debug server's main thread now returns from waitpid(). It calls
//    checkInterrupt() with the results from waitpid(). The call compares the
//    pid returned by waitpid() with the pid stashed by sendInterrupt(), which
//    returns true if pid matches the one saved during the last call to
//    sendInterrupt().
//
// NOTE: There can be only one pending interrupt at a time. Subsequent calls to
// sendInterrupt() without corresponding calls to checkInterrupt() are no-ops.
//
class WaitInterrupter {
private:
  // Mutex guards against concurrent calls to checkInterrupt and sendInterrupt.
  // It is required to atomically fork the interrupting process and memoize it's
  // pid in the _pid field.
  std::mutex _mutex;
  ProcessId _pid = 0;

public:
  bool checkInterrupt(ThreadId tid, int waitStatus);
  ErrorCode sendInterrupt();
};

}
}
}
