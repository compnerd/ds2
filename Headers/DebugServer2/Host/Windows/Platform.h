//
// Copyright (c) 2014, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#ifndef __DebugServer2_Host_Windows_Platform_h
#define __DebugServer2_Host_Windows_Platform_h

#ifndef __DebugServer2_Host_Platform_h
#error "You shall not include this file directly."
#endif

namespace ds2 {
namespace Host {
namespace Windows {

class Platform {
public:
  static void Initialize();

public:
  static CPUType GetCPUType();
  static CPUSubType GetCPUSubType();

public:
  static Endian GetEndian();

public:
  static size_t GetPointerSize();

public:
  static const char *GetHostName();

public:
  static const char *GetOSTypeName();
  static const char *GetOSVendorName();
  static const char *GetOSVersion();
  static const char *GetOSBuild();
  static const char *GetOSKernelPath();

public:
  static bool GetUserName(UserId const &uid, std::string &name);
  static bool GetGroupName(GroupId const &gid, std::string &name);

public:
  static bool IsFilePresent(std::string const &path);

public:
  static ProcessId GetCurrentProcessId();

public:
  static bool GetProcessInfo(ProcessId pid, ProcessInfo &info);
  static void
  EnumerateProcesses(bool allUsers, UserId const &uid,
                     std::function<void(ProcessInfo const &info)> const &cb);

public:
  static std::string GetThreadName(ProcessId pid, ThreadId tid);

public:
  static const char *GetSelfExecutablePath();
};
}
}
}

#endif // !__DebugServer2_Host_Windows_Platform_h
