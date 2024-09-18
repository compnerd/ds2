#include "DebugServer2/Host/Platform.h"
#include "DebugServer2/Host/POSIX/WaitInterrupter.h"
#include "DebugServer2/Utils/Log.h"

namespace ds2 {
namespace Host {
namespace POSIX {

bool WaitInterrupter::checkInterrupt(ThreadId tid, int waitStatus) {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_pid <= 0) // no interrupt
      return false;

    if (_pid != tid || !WIFEXITED(waitStatus)) // tid does not match interrupt
      return false;

    _pid = 0; // clear interrupt
  }

  DS2LOG(Debug, "received interrupt from process %" PRI_PID, tid);
  return true;
}

ErrorCode WaitInterrupter::sendInterrupt() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_pid > 0) // There is already a pending interrupt
    return kSuccess;

  const ProcessId pid = ::fork();
  if (pid < 0)
    return Platform::TranslateError();

  if (pid == 0) {
    DS2LOG(Debug, "forked process %" PRI_PID " to interrupt waiter",
           ::getpid());
    // Exiting the forked thread will wake the waiting parent process with
    // WIFEXITED status.
    exit(0);
  }

  _pid = pid;
  return kSuccess;
}

}
}
}
