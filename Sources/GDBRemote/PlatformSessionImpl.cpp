//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/GDBRemote/PlatformSessionImpl.h"
#include "DebugServer2/GDBRemote/Session.h"
#include "DebugServer2/Host/Platform.h"
#include "DebugServer2/Host/ProcessSpawner.h"
#include "DebugServer2/Utils/Log.h"

using ds2::Host::ProcessSpawner;

namespace ds2 {
namespace GDBRemote {

PlatformSessionImplBase::PlatformSessionImplBase()
    : DummySessionDelegateImpl() {}

ErrorCode PlatformSessionImplBase::onQueryProcessList(
    Session &session, ProcessInfoMatch const &match, bool first,
    ProcessInfo &info) const {
  if (first) {
    updateProcesses(match);
  }

  if (_processIterationState.it == _processIterationState.vals.end())
    return kErrorProcessNotFound;

  if (!Platform::GetProcessInfo(*_processIterationState.it++, info))
    return kErrorProcessNotFound;

  return kSuccess;
}

ErrorCode
PlatformSessionImplBase::onQueryProcessInfoPID(Session &, ProcessId pid,
                                               ProcessInfo &info) const {
  if (!Platform::GetProcessInfo(pid, info))
    return kErrorProcessNotFound;
  else
    return kSuccess;
}

ErrorCode PlatformSessionImplBase::onExecuteProgram(
    Session &, std::string const &command, uint32_t timeout,
    std::string const &workingDirectory, ProgramResult &result) {
  DS2LOG(Debug, "command='%s' timeout=%u", command.c_str(), timeout);

  ProcessSpawner ps;

  ps.setShellCommand(command);

  ps.redirectInputToNull();
  ps.redirectOutputToBuffer();
  ps.redirectErrorToBuffer();
  ps.setWorkingDirectory(workingDirectory);

  ErrorCode error;
  error = ps.run();
  if (error != kSuccess)
    return error;
  error = ps.wait();
  if (error != kSuccess)
    return error;

  result.status = ps.exitStatus();
  result.signal = ps.signalCode();
  result.output = ps.output();

  return kSuccess;
}

ErrorCode PlatformSessionImplBase::onQueryUserName(Session &, UserId const &uid,
                                                   std::string &name) const {
  if (!Platform::GetUserName(uid, name))
    return kErrorNotFound;
  else
    return kSuccess;
}

ErrorCode PlatformSessionImplBase::onQueryGroupName(Session &,
                                                    GroupId const &gid,
                                                    std::string &name) const {
  if (!Platform::GetGroupName(gid, name))
    return kErrorNotFound;
  else
    return kSuccess;
}

ErrorCode PlatformSessionImplBase::onLaunchDebugServer(Session &session,
                                                       std::string const &host,
                                                       uint16_t &port,
                                                       ProcessId &pid) {
  ProcessSpawner ps;
  StringCollection args;

  ps.setExecutable(Platform::GetSelfExecutablePath());
  args.push_back("slave");
  if (GetLogLevel() == kLogLevelDebug) {
    args.push_back("--debug");
  } else if (GetLogLevel() == kLogLevelPacket) {
    args.push_back("--remote-debug");
  }
  std::string const &outputFilename = GetLogOutputFilename();
  if (outputFilename.length() > 0) {
    args.push_back("--log-file");
    args.push_back(outputFilename);
  }
#if defined(OS_POSIX)
  args.push_back("--setsid");
#endif
  ps.setArguments(args);
  ps.redirectInputToNull();
  ps.redirectOutputToBuffer();

  ErrorCode error;
  error = ps.run();
  if (error != kSuccess)
    return error;
  error = ps.wait();
  if (error != kSuccess)
    return error;

  if (ps.exitStatus() != 0)
    return kErrorInvalidArgument;

  std::istringstream ss;
  ss.str(ps.output());
  ss >> port >> pid;

  return kSuccess;
}

void PlatformSessionImplBase::updateProcesses(
    ProcessInfoMatch const &match) const {
  _processIterationState.vals.clear();
  Platform::EnumerateProcesses(
      true, UserId(), [&](ds2::ProcessInfo const &info) {
        if (processMatch(match, info))
          _processIterationState.vals.push_back(info.pid);
      });

  _processIterationState.it = _processIterationState.vals.begin();
}

bool PlatformSessionImplBase::processMatch(ProcessInfoMatch const &match,
                                           ds2::ProcessInfo const &info) {
  // Allow matching against the process info name, which is the full executable
  // file path on Linux, or the name of the primary thread.
  if (!match.name.empty() &&
      !nameMatch(match, info.name) &&
      !nameMatch(match, Platform::GetThreadName(info.pid, info.pid)))
    return false;

  if (match.pid != 0 && info.pid != match.pid)
      return false;

#if !defined(OS_WIN32)
  if (match.parentPid != 0 && info.parentPid != match.parentPid)
      return false;

  if (match.effectiveUid != 0 && info.effectiveUid != match.effectiveUid)
    return false;

  if (match.effectiveGid != 0 && info.effectiveGid != match.effectiveGid)
    return false;
#endif

  if (match.realGid != 0 && info.realGid != match.realGid)
      return false;

  if (match.realUid != 0 && info.realUid != match.realUid)
      return false;

  // TODO(andrurogerz): account for match.triple
  return true;
}

bool PlatformSessionImplBase::nameMatch(ProcessInfoMatch const &match,
                                        std::string const &name) {
  if (match.nameMatch == "equals")
    return name.compare(match.name) == 0;

  if (match.nameMatch == "starts_with")
    return name.size() >= match.name.size() &&
           name.compare(0, match.name.size(), match.name) == 0;

  if (match.nameMatch == "ends_with")
    return name.size() >= match.name.size() &&
           name.compare(name.size() - match.name.size(),
                        match.name.size(),
                        match.name) == 0;

  if (match.nameMatch == "contains")
    return name.rfind(match.name) != std::string::npos;

  if (match.nameMatch == "regex")
    // TODO(andrurogerz): match against "regex"
    DS2LOG(Error, "name_match:regex is not currently supported");

  return true;
}
} // namespace GDBRemote
} // namespace ds2
