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

#include "DebugServer2/Types.h"

#include <array>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ds2 {
namespace Host {

class ProcessSpawner {
protected:
  enum class RedirectMode {
    unset,
    console,
    null,
    file,
    buffer,
  };

protected:
  // Named indices into _descriptors/_redirectThreads and the parallel local
  // arrays run() builds up, one per standard stream.
  enum class StreamIndex {
    input,
    output,
    error,
    count,
  };

  // StreamIndex doesn't implicitly convert to the array index types
  // (size_t/DWORD) it is used with; this is the one place that conversion
  // happens.
  static constexpr size_t Index(StreamIndex stream) {
    return static_cast<size_t>(stream);
  }

  // Deliberately not Index(StreamIndex::count): MSVC's C2131 rejects a
  // static data member's in-class initializer calling a member function of
  // the still-incomplete enclosing class, even a constexpr one.
  static constexpr size_t kStreamCount =
      static_cast<size_t>(StreamIndex::count);

protected:
  typedef std::function<void(void *buf, size_t size)> RedirectDelegate;

protected:
  struct RedirectDescriptor {
    RedirectMode mode;
    std::string path;
    // The parent's end of a pipe, valid only for RedirectMode::buffer
    // streams while the redirection thread is reading from it.
    HANDLE handle;

    RedirectDescriptor() : mode(RedirectMode::unset), handle(nullptr) {}
  };

protected:
  std::string _executablePath;
  StringCollection _arguments;
  // Appended to the command line completely verbatim, after all quoted
  // _arguments -- used only by setShellCommand(). cmd.exe re-parses its
  // command text itself (via its own separate, and separately quirky, /C
  // handling) rather than treating it as a normal, quoted argv element, so
  // it needs to reach cmd.exe exactly as given rather than quoted the way a
  // standard argv-consuming program would expect.
  std::string _rawTrailingArgument;
  EnvironmentBlock _environment;
  // Whether setEnvironment()/addEnvironment() was ever called, as opposed
  // to _environment merely being empty. A caller can legitimately want an
  // explicitly empty launch environment (ProcessLaunchMixin always calls
  // setEnvironment(), even with nothing accumulated in it); that must still
  // reach CreateProcessW as a valid, empty environment block, not collapse
  // to "no environment was configured, inherit ours" the way run() treats
  // callers (onLaunchDebugServer, onExecuteProgram) that never call either
  // method at all.
  bool _environmentSet;
  std::string _workingDirectory;
  std::array<RedirectDescriptor, kStreamCount> _descriptors;
  // One reader thread per RedirectMode::buffer stream (there can be up to two,
  // output and error, draining concurrently -- a single multiplexed reader
  // would risk a classic pipe deadlock: blocked reading a currently-empty
  // stream while the child blocks writing to the other stream's full pipe).
  std::array<std::thread, kStreamCount> _redirectThreads;
  std::mutex _outputBufferMutex;
  std::string _outputBuffer;
  bool _debugOnCreate;
  int _exitStatus;
  ProcessId _pid;
  HANDLE _handle;
  ThreadId _tid;
  HANDLE _threadHandle;

public:
  ProcessSpawner();
  ~ProcessSpawner();

public:
  ProcessSpawner(const ProcessSpawner &) = delete;
  ProcessSpawner &operator=(const ProcessSpawner &) = delete;

public:
  bool setExecutable(const std::string &path);
  bool setShellCommand(const std::string &command);
  bool setWorkingDirectory(const std::string &path);

public:
  bool setArguments(const StringCollection &args);

  template <typename... Args> inline bool setArguments(const Args &...args) {
    std::string args_[] = {args...};
    return setArguments(StringCollection(&args_[0], &args_[sizeof...(Args)]));
  }

public:
  bool setEnvironment(const EnvironmentBlock &args);
  bool addEnvironment(const std::string &key, const std::string &val);

public:
  // Whether the created process should be started as a debuggee
  // (DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS). Defaults to false. Only
  // Target::Windows::Process needs this set: it is the only caller that goes
  // on to pump WaitForDebugEvent/ContinueDebugEvent for the child. Helper
  // processes spawned to service platform-mode RPCs (launching a slave debug
  // server, running a shell command, ...) must not be debugged, since
  // nothing would ever continue them past their initial debug event and
  // they would hang forever.
  void setDebugOnCreate(bool debug) { _debugOnCreate = debug; }

public:
  bool redirectInputToConsole();
  bool redirectOutputToConsole();
  bool redirectErrorToConsole();

public:
  bool redirectInputToNull();
  bool redirectOutputToNull();
  bool redirectErrorToNull();

public:
  bool redirectInputToFile(const std::string &path);
  bool redirectOutputToFile(const std::string &path);
  bool redirectErrorToFile(const std::string &path);

public:
  bool redirectOutputToBuffer();
  bool redirectErrorToBuffer();

public:
  bool redirectInputToTerminal() { return false; }

public:
  bool redirectOutputToDelegate(RedirectDelegate delegate) { return false; }
  bool redirectErrorToDelegate(RedirectDelegate delegate) { return false; }

public:
  ErrorCode run(std::function<bool()> preExecAction = []() { return true; });
  ErrorCode wait();
  bool isRunning() const;
  void flushAndExit();

public:
  inline ProcessId pid() const { return _pid; }
  inline HANDLE handle() const { return _handle; }
  inline ThreadId tid() const { return _tid; }
  inline HANDLE threadHandle() const { return _threadHandle; }
  inline int exitStatus() const { return _exitStatus; }
  inline int signalCode() const { return 0; }
  inline const std::string &executable() const { return _executablePath; }
  inline const StringCollection &arguments() const { return _arguments; }
  inline const EnvironmentBlock &environment() const { return _environment; }

public:
  ErrorCode input(const ByteVector &buf) { return kErrorUnsupported; }

  inline const std::string &output() const { return _outputBuffer; }

private:
  void redirectionThread(size_t index);
};
} // namespace Host
} // namespace ds2
