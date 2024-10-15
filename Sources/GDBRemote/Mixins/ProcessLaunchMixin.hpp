//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#pragma once

#include "DebugServer2/GDBRemote/Mixins/ProcessLaunchMixin.h"

#include <algorithm>

namespace ds2 {
namespace GDBRemote {

template <typename T>
ErrorCode ProcessLaunchMixin<T>::onDisableASLR(Session &session, bool disable) {
  _disableASLR = disable;
  return kSuccess;
}

template <typename T>
ErrorCode
ProcessLaunchMixin<T>::onSetArchitecture(Session &,
                                         std::string const &architecture) {
  // Ignored for now.
  return kSuccess;
}

template <typename T>
ErrorCode
ProcessLaunchMixin<T>::onSetWorkingDirectory(Session &,
                                             std::string const &path) {
  _workingDirectory = path;
  return kSuccess;
}

template <typename T>
ErrorCode
ProcessLaunchMixin<T>::onQueryWorkingDirectory(Session &,
                                               std::string &workingDir) const {
  workingDir = _workingDirectory;
  return kSuccess;
}

template <typename T>
ErrorCode ProcessLaunchMixin<T>::onSetEnvironmentVariable(Session &,
    std::string const &name, std::string const &value) {
  if (value.empty()) {
    _environment.erase(name);
  } else {
    _environment.insert(std::make_pair(name, value));
  }
  return kSuccess;
}

template <typename T>
ErrorCode ProcessLaunchMixin<T>::onSetStdFile(Session &, int fileno,
                                              std::string const &path) {
  DS2LOG(Debug, "stdfile[%d] = %s", fileno, path.c_str());

  if (fileno < 0 || fileno > 2)
    return kErrorInvalidArgument;

  if (fileno == 0) {
    //
    // TODO it seems that QSetSTDIN is the first message sent when
    // launching a new process, it may be sane to invalidate all
    // the process state at this point.
    //
    _disableASLR = false;
    _arguments.clear();
    _environment.clear();
    _workingDirectory.clear();
    _stdFile[0].clear();
    _stdFile[1].clear();
    _stdFile[2].clear();
  }

  _stdFile[fileno] = path;
  return kSuccess;
}

template <typename T>
ErrorCode
ProcessLaunchMixin<T>::onSetProgramArguments(Session &,
                                             StringCollection const &args) {
  _arguments = args;
  if (isDebugServer(args)) {
    // Propagate current logging settings when launching an instance of
    // the debug server to match qLaunchGDBServer behavior.
    if (GetLogLevel() == kLogLevelDebug)
      _arguments.push_back("--debug");
    else if (GetLogLevel() == kLogLevelPacket)
      _arguments.push_back("--remote-debug");
  }

  return spawnProcess();

}

template <typename T>
ErrorCode ProcessLaunchMixin<T>::onQueryLaunchSuccess(Session &,
                                                      ProcessId) const {
  return _lastLaunchResult;
}

template <typename T>
ErrorCode
ProcessLaunchMixin<T>::onQueryProcessInfo(Session &,
                                          ProcessInfo &info) const {
  if (_processList.empty())
    return kErrorProcessNotFound;

  const ProcessId pid = _processList.back();
  if (!Platform::GetProcessInfo(pid, info))
    return kErrorUnknown;

  return kSuccess;
}

template <typename T>
ErrorCode ProcessLaunchMixin<T>::onTerminate(Session &session,
                                             ProcessThreadId const &ptid,
                                             StopInfo &stop) {
  auto it = std::find(_processList.begin(), _processList.end(), ptid.pid);
  if (it == _processList.end())
    return kErrorNotFound;

  if (!Platform::TerminateProcess(ptid.pid))
    return kErrorUnknown;

  DS2LOG(Debug, "killed spawned process %" PRI_PID, *it);
  _processList.erase(it);

  // StopInfo is not populated by this implemenation of onTerminate since it is
  // only ever called in the context of a platform session in response to a
  // qKillSpawnedProcess packet.
  stop.clear();
  return kSuccess;
}

// Some test cases use this code path to launch additional instances of the
// debug server rather than sending a qLaunchGDBServer packet. Allow detection
// of this scenario so we can propagate logging settings and make debugging
// test failures easier.
template <typename T>
bool ProcessLaunchMixin<T>::isDebugServer(StringCollection const &args) {
  return (args.size() > 1)
      && (args[0] == Platform::GetSelfExecutablePath())
      && (args[1] == "gdbserver");
}

template <typename T>
ErrorCode ProcessLaunchMixin<T>::spawnProcess() {
  const bool displayArgs = _arguments.size() > 1;
  const bool displayEnv = !_environment.empty();
  auto it = _arguments.begin();
  DS2LOG(Debug, "spawning process '%s'%s", (it++)->c_str(),
         displayArgs ? " with args:" : "");
  while (it != _arguments.end()) {
    DS2LOG(Debug, "  %s", (it++)->c_str());
  }

  if (displayEnv) {
    DS2LOG(Debug, "%swith environment:", displayArgs ? "and " : "");
    for (auto const &val : _environment) {
      DS2LOG(Debug, "  %s=%s", val.first.c_str(), val.second.c_str());
    }
  }

  if (!_workingDirectory.empty()) {
    DS2LOG(Debug, "%swith working directory: %s",
           displayArgs || displayEnv ? "and " : "",
           _workingDirectory.c_str());
  }

  Host::ProcessSpawner ps;
  if (!ps.setExecutable(_arguments[0]) ||
      !ps.setArguments(StringCollection(_arguments.begin() + 1,
                                        _arguments.end())) ||
      !ps.setWorkingDirectory(_workingDirectory) ||
      !ps.setEnvironment(_environment))
    return kErrorInvalidArgument;

  if (isDebugServer(_arguments)) {
    // Always log to the console when launching an instance of the debug server
    // to match qLaunchGDBServer behavior.
    ps.redirectInputToNull();
    ps.redirectOutputToConsole();
    ps.redirectErrorToConsole();
  } else {
    if (!_stdFile[0].empty() && !ps.redirectInputToFile(_stdFile[0]))
      return kErrorInvalidArgument;
    if (!_stdFile[1].empty() && !ps.redirectOutputToFile(_stdFile[1]))
      return kErrorInvalidArgument;
    if (!_stdFile[2].empty() && !ps.redirectErrorToFile(_stdFile[2]))
      return kErrorInvalidArgument;
  }

  _lastLaunchResult = ps.run();
  if (_lastLaunchResult != kSuccess)
    return _lastLaunchResult;

  // Add the pid to the list of launched processes. It will be removed when
  // onTerminate is called.
  _processList.push_back(ps.pid());

  DS2LOG(Debug, "launched %s as process %" PRI_PID, _arguments[0].c_str(),
         ps.pid());

  return kSuccess;
}
} // namespace GDBRemote
} // namespace ds2
