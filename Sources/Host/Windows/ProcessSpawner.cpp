//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Host/ProcessSpawner.h"
#include "DebugServer2/Host/Platform.h"
#include "DebugServer2/Host/Windows/ExtraWrappers.h"
#include "DebugServer2/Utils/Log.h"
#include "DebugServer2/Utils/String.h"

#include <algorithm>
#include <mutex>
#include <vector>
#include <windows.h>

using ds2::Host::Platform;

namespace ds2 {
namespace Host {

ProcessSpawner::ProcessSpawner()
    : _environmentSet(false), _debugOnCreate(false), _exitStatus(0), _pid(0),
      _handle(nullptr), _tid(0), _threadHandle(nullptr) {}

ProcessSpawner::~ProcessSpawner() { flushAndExit(); }

void ProcessSpawner::flushAndExit() {
  for (auto &thread : _redirectThreads)
    if (thread.joinable())
      thread.join();

  // wait() already closes and nulls these out; if it was never called (the
  // common case for platform-mode helper launches that only ever need the
  // pid, e.g. ProcessLaunchMixin::spawnProcess()), these handles would
  // otherwise stay open for the entire remaining lifetime of this ds2
  // process -- one leaked pair per launch, on a long-lived platform server.
  if (_threadHandle != nullptr) {
    ::CloseHandle(_threadHandle);
    _threadHandle = nullptr;
  }
  if (_handle != nullptr) {
    ::CloseHandle(_handle);
    _handle = nullptr;
  }
}

bool ProcessSpawner::setExecutable(const std::string &path) {
  if (_pid != 0)
    return false;

  _executablePath = path;
  return true;
}

bool ProcessSpawner::setShellCommand(const std::string &command) {
  if (_pid != 0)
    return false;

  // Unlike POSIX's execve(), CreateProcessW never interprets its command
  // line -- setExecutable(command) would make the whole shell command (e.g.
  // "echo hi" or "dir /w") a single, literal executable name to look up,
  // which fails for anything but a bare, argument-less program. Route
  // through the configured shell instead, exactly as one would type
  // `%COMSPEC% /C <command>` -- the same thing setShellCommand's POSIX
  // counterpart does with `sh -c`.
  std::wstring comspec;
  DWORD size = ::GetEnvironmentVariableW(L"COMSPEC", nullptr, 0);
  if (size > 0) {
    comspec.resize(size);
    comspec.resize(::GetEnvironmentVariableW(L"COMSPEC", &comspec[0], size));
  }

  setExecutable(comspec.empty() ? "cmd.exe"
                                : ds2::Utils::WideToNarrowString(comspec));
  setArguments(StringCollection{"/C"});
  // Not a normal, quoted _arguments element -- see _rawTrailingArgument.
  _rawTrailingArgument = command;
  return true;
}

bool ProcessSpawner::setArguments(const StringCollection &args) {
  if (_pid != 0)
    return false;

  _arguments = args;
  return true;
}

bool ProcessSpawner::setEnvironment(const EnvironmentBlock &env) {
  if (_pid != 0)
    return false;

  _environment = env;
  _environmentSet = true;
  return true;
}

bool ProcessSpawner::addEnvironment(const std::string &key,
                                    const std::string &val) {
  if (_pid != 0)
    return false;

  _environment[key] = val;
  _environmentSet = true;
  return true;
}

bool ProcessSpawner::setWorkingDirectory(const std::string &path) {
  if (_pid != 0)
    return false;

  _workingDirectory = path;
  return true;
}

//
// Redirection setup -- these only record intent; run() does the actual work.
//
bool ProcessSpawner::redirectInputToConsole() {
  if (_pid != 0 ||
      _descriptors[Index(StreamIndex::input)].mode != RedirectMode::unset)
    return false;

  _descriptors[Index(StreamIndex::input)] = RedirectDescriptor();
  _descriptors[Index(StreamIndex::input)].mode = RedirectMode::console;
  return true;
}

bool ProcessSpawner::redirectOutputToConsole() {
  if (_pid != 0 ||
      _descriptors[Index(StreamIndex::output)].mode != RedirectMode::unset)
    return false;

  _descriptors[Index(StreamIndex::output)] = RedirectDescriptor();
  _descriptors[Index(StreamIndex::output)].mode = RedirectMode::console;
  return true;
}

bool ProcessSpawner::redirectErrorToConsole() {
  if (_pid != 0 ||
      _descriptors[Index(StreamIndex::error)].mode != RedirectMode::unset)
    return false;

  _descriptors[Index(StreamIndex::error)] = RedirectDescriptor();
  _descriptors[Index(StreamIndex::error)].mode = RedirectMode::console;
  return true;
}

bool ProcessSpawner::redirectInputToNull() {
  if (_pid != 0 ||
      _descriptors[Index(StreamIndex::input)].mode != RedirectMode::unset)
    return false;

  _descriptors[Index(StreamIndex::input)] = RedirectDescriptor();
  _descriptors[Index(StreamIndex::input)].mode = RedirectMode::null;
  return true;
}

bool ProcessSpawner::redirectOutputToNull() {
  if (_pid != 0 ||
      _descriptors[Index(StreamIndex::output)].mode != RedirectMode::unset)
    return false;

  _descriptors[Index(StreamIndex::output)] = RedirectDescriptor();
  _descriptors[Index(StreamIndex::output)].mode = RedirectMode::null;
  return true;
}

bool ProcessSpawner::redirectErrorToNull() {
  if (_pid != 0 ||
      _descriptors[Index(StreamIndex::error)].mode != RedirectMode::unset)
    return false;

  _descriptors[Index(StreamIndex::error)] = RedirectDescriptor();
  _descriptors[Index(StreamIndex::error)].mode = RedirectMode::null;
  return true;
}

bool ProcessSpawner::redirectInputToFile(const std::string &path) {
  if (_pid != 0 || path.empty() ||
      _descriptors[Index(StreamIndex::input)].mode != RedirectMode::unset)
    return false;

  _descriptors[Index(StreamIndex::input)] = RedirectDescriptor();
  _descriptors[Index(StreamIndex::input)].mode = RedirectMode::file;
  _descriptors[Index(StreamIndex::input)].path = path;
  return true;
}

bool ProcessSpawner::redirectOutputToFile(const std::string &path) {
  if (_pid != 0 || path.empty() ||
      _descriptors[Index(StreamIndex::output)].mode != RedirectMode::unset)
    return false;

  _descriptors[Index(StreamIndex::output)] = RedirectDescriptor();
  _descriptors[Index(StreamIndex::output)].mode = RedirectMode::file;
  _descriptors[Index(StreamIndex::output)].path = path;
  return true;
}

bool ProcessSpawner::redirectErrorToFile(const std::string &path) {
  if (_pid != 0 || path.empty() ||
      _descriptors[Index(StreamIndex::error)].mode != RedirectMode::unset)
    return false;

  _descriptors[Index(StreamIndex::error)] = RedirectDescriptor();
  _descriptors[Index(StreamIndex::error)].mode = RedirectMode::file;
  _descriptors[Index(StreamIndex::error)].path = path;
  return true;
}

bool ProcessSpawner::redirectOutputToBuffer() {
  if (_pid != 0 ||
      _descriptors[Index(StreamIndex::output)].mode != RedirectMode::unset)
    return false;

  _descriptors[Index(StreamIndex::output)] = RedirectDescriptor();
  _descriptors[Index(StreamIndex::output)].mode = RedirectMode::buffer;
  return true;
}

bool ProcessSpawner::redirectErrorToBuffer() {
  if (_pid != 0 ||
      _descriptors[Index(StreamIndex::error)].mode != RedirectMode::unset)
    return false;

  _descriptors[Index(StreamIndex::error)] = RedirectDescriptor();
  _descriptors[Index(StreamIndex::error)].mode = RedirectMode::buffer;
  return true;
}

namespace {

bool NeedsQuoting(const std::wstring &arg) {
  return arg.empty() || arg.find_first_of(L" \t\"") != std::wstring::npos;
}

std::wstring BuildCommandLine(const std::string &executablePath,
                              const StringCollection &arguments,
                              const std::string &rawTrailingArgument) {
  std::wstring commandLine;
  commandLine += L'"';
  commandLine += ds2::Utils::NarrowToWideString(executablePath);
  commandLine += L'"';
  for (const auto &arg : arguments) {
    commandLine += L' ';
    std::wstring wideArg = ds2::Utils::NarrowToWideString(arg);
    if (!NeedsQuoting(wideArg)) {
      // Leave arguments that plainly don't need it (e.g. cmd.exe's "/C")
      // unquoted -- harmless for a standard argv-consuming program (an
      // unquoted single word parses identically to a quoted one), and it
      // keeps cmd.exe's own switch scan (which works off the raw command
      // line text, not a parsed argv) recognizing "/C" as the switch it is.
      commandLine += wideArg;
      continue;
    }

    commandLine += L'"';
    for (const auto &ch : wideArg) {
      if (ch == L'"')
        commandLine += L'\\';
      commandLine += ch;
    }
    commandLine += L'"';
  }

  if (!rawTrailingArgument.empty()) {
    // setShellCommand()'s command text, appended completely verbatim rather
    // than quoted like a normal argv element. cmd.exe re-parses whatever
    // follows "/C" itself, via its own separate (and separately quirky)
    // rules for when to strip a surrounding quote pair back off; quoting
    // this the same way a standard argv-consuming program would want is
    // unnecessary (cmd.exe isn't one) and, for anything that is itself a
    // nested "cmd /c ..." invocation, actively wrong -- verified empirically
    // against real cmd.exe rather than derived from its under-documented
    // stripping algorithm.
    commandLine += L' ';
    commandLine += ds2::Utils::NarrowToWideString(rawTrailingArgument);
  }

  return commandLine;
}

std::vector<WCHAR> BuildEnvironmentBlock(const EnvironmentBlock &env) {
  std::vector<WCHAR> environment;
  for (const auto &e : env) {
    std::wstring wideKey = ds2::Utils::NarrowToWideString(e.first);
    std::wstring wideValue = ds2::Utils::NarrowToWideString(e.second);
    environment.insert(environment.end(), wideKey.begin(), wideKey.end());
    environment.push_back(L'=');
    environment.insert(environment.end(), wideValue.begin(), wideValue.end());
    environment.push_back(L'\0');
  }
  // A Windows environment block must end with an extra null marking the end
  // of the list. For a non-empty block the loop above already left one
  // trailing null, so the push below completes the required double-null
  // terminator. For an EMPTY EnvironmentBlock, the loop never runs, so
  // without this an empty block would collapse to a single null, which
  // CreateProcessW rejects with ERROR_INVALID_PARAMETER when
  // CREATE_UNICODE_ENVIRONMENT is set -- push a second one in that case so
  // the block is a valid (if empty) list instead.
  if (environment.empty())
    environment.push_back(L'\0');
  environment.push_back(L'\0');
  return environment;
}

} // namespace

ErrorCode ProcessSpawner::run(std::function<bool()> preExecAction) {
  // preExecAction is a POSIX-only concept (it runs in the forked child right
  // before exec, e.g. to call ptrace's traceme). CreateProcessW has no
  // equivalent split point, so, as before this function did anything at all,
  // it is intentionally not invoked here.
  static_cast<void>(preExecAction);

  if (_pid != 0 || _executablePath.empty())
    return kErrorInvalidArgument;

  bool anyExplicitRedirect =
      std::any_of(_descriptors.begin(), _descriptors.end(),
                  [](const RedirectDescriptor &d) {
                    return d.mode != RedirectMode::unset;
                  });

  std::wstring commandLine =
      BuildCommandLine(_executablePath, _arguments, _rawTrailingArgument);
  std::vector<WCHAR> environment = BuildEnvironmentBlock(_environment);
  // Callers that never called setEnvironment()/addEnvironment() at all
  // (onLaunchDebugServer and onExecuteProgram, notably) want "inherit
  // whatever ds2 itself is running under" -- pass nullptr in that case so
  // the child inherits our environment, matching plain CreateProcessW
  // behavior. That is deliberately not the same test as "_environment is
  // empty": ProcessLaunchMixin::spawnProcess() always calls
  // setEnvironment(), even when nothing has been accumulated in it, and an
  // explicitly empty launch environment (matching what the POSIX execve
  // path does with the same empty EnvironmentMap) must still reach
  // CreateProcessW as a valid, empty block rather than collapse to "inherit
  // ours". A genuinely empty block isn't the same as passing nullptr,
  // either: many system components (Winsock among them) implicitly rely on
  // standard environment variables (e.g. SystemRoot) being present, so
  // BuildEnvironmentBlock() always produces a properly double-null-
  // terminated (if otherwise empty) block rather than an empty vector.
  LPVOID environmentArg =
      _environmentSet ? static_cast<LPVOID>(environment.data()) : nullptr;
  std::wstring wideWorkingDirectory =
      ds2::Utils::NarrowToWideString(_workingDirectory);
  LPCWSTR workingDirectoryArg =
      wideWorkingDirectory.empty() ? nullptr : wideWorkingDirectory.c_str();

  DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;
  if (_debugOnCreate)
    creationFlags |= DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS;

  PROCESS_INFORMATION pi = {};

  // Reproduce a plain, non-redirected CreateProcessW invocation with no
  // inherited handles at all, exactly as this function has always done.
  // This keeps Target::Windows::Process's debuggee-launch path (which never
  // successfully configures redirection today -- DebugSessionImpl only
  // calls the terminal/delegate redirection methods, which remain
  // unimplemented stubs) completely unaffected by the redirection machinery
  // below.
  auto launchPlain = [&]() -> ErrorCode {
    STARTUPINFOW si = {};
    si.cb = sizeof(si);

    BOOL result = ::CreateProcessW(
        nullptr, &commandLine[0], nullptr, nullptr, /*bInheritHandles=*/FALSE,
        creationFlags, environmentArg, workingDirectoryArg, &si, &pi);
    if (!result)
      return Platform::TranslateError();

    _pid = pi.dwProcessId;
    _handle = pi.hProcess;
    _tid = pi.dwThreadId;
    _threadHandle = pi.hThread;
    return kSuccess;
  };

  if (!anyExplicitRedirect)
    return launchPlain();

  // At least one stream was explicitly redirected. Resolve each of the three
  // standard streams to a handle the child will receive. Streams left
  // RedirectMode::unset are resolved the same way as RedirectMode::console
  // (pass the process's own current standard handle through), since
  // STARTF_USESTDHANDLES applies to all three streams at once.
  std::array<HANDLE, kStreamCount> stdHandles = {};
  std::array<bool, kStreamCount> ownsStdHandle = {};
  std::array<HANDLE, kStreamCount> parentPipeReadEnd = {};

  auto cleanupHandles = [&]() {
    for (size_t n = 0; n < kStreamCount; n++) {
      if (ownsStdHandle[n] && stdHandles[n] != nullptr)
        ::CloseHandle(stdHandles[n]);
      if (parentPipeReadEnd[n] != nullptr)
        ::CloseHandle(parentPipeReadEnd[n]);
    }
  };

  for (size_t n = 0; n < kStreamCount; n++) {
    RedirectDescriptor &d = _descriptors[n];
    switch (d.mode) {
    case RedirectMode::unset:
    case RedirectMode::console: {
      DWORD which = (n == Index(StreamIndex::input))    ? STD_INPUT_HANDLE
                    : (n == Index(StreamIndex::output)) ? STD_OUTPUT_HANDLE
                                                        : STD_ERROR_HANDLE;
      HANDLE handle = ::GetStdHandle(which);
      if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
        ::SetHandleInformation(handle, HANDLE_FLAG_INHERIT,
                               HANDLE_FLAG_INHERIT);
        stdHandles[n] = handle;
      }
      break;
    }

    case RedirectMode::null: {
      SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
      DWORD access =
          (n == Index(StreamIndex::input)) ? GENERIC_READ : GENERIC_WRITE;
      HANDLE handle =
          ::CreateFileW(L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (handle == INVALID_HANDLE_VALUE) {
        DS2LOG(Warning, "RedirectMode::null n=%zu failed err=%#lx", n,
               ::GetLastError());
        cleanupHandles();
        return Platform::TranslateError();
      }
      stdHandles[n] = handle;
      ownsStdHandle[n] = true;
      break;
    }

    case RedirectMode::file: {
      // If stderr is being redirected to the exact same path as stdout,
      // duplicate that already-open handle instead of opening the path a
      // second time. Two independent handles opened separately (even with
      // FILE_SHARE_WRITE) would each start writing from offset 0, silently
      // clobbering whatever the other had already written, since neither
      // handle's file position is aware of the other; sharing one handle
      // means both streams append to wherever the current position
      // actually is.
      if (n == Index(StreamIndex::error) &&
          _descriptors[Index(StreamIndex::output)].mode == RedirectMode::file &&
          _descriptors[Index(StreamIndex::output)].path == d.path &&
          stdHandles[Index(StreamIndex::output)] != nullptr) {
        HANDLE dupHandle = nullptr;
        if (!::DuplicateHandle(::GetCurrentProcess(),
                               stdHandles[Index(StreamIndex::output)],
                               ::GetCurrentProcess(), &dupHandle, 0, TRUE,
                               DUPLICATE_SAME_ACCESS)) {
          DS2LOG(Warning,
                 "DuplicateHandle for shared stdout/stderr file failed "
                 "err=%#lx",
                 ::GetLastError());
          cleanupHandles();
          return Platform::TranslateError();
        }
        stdHandles[n] = dupHandle;
        ownsStdHandle[n] = true;
        break;
      }

      SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
      std::wstring widePath = ds2::Utils::NarrowToWideString(d.path);
      // FILE_SHARE_WRITE on the write side isn't just for the stdout/stderr
      // duplicate-handle case above (which never opens the path twice to
      // begin with): it also covers, e.g., a client redirecting output to a
      // file it or another handle already has open elsewhere.
      HANDLE handle =
          (n == Index(StreamIndex::input))
              ? ::CreateFileW(widePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr)
              : ::CreateFileW(widePath.c_str(), GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (handle == INVALID_HANDLE_VALUE) {
        DS2LOG(Warning, "RedirectMode::file n=%zu failed err=%#lx", n,
               ::GetLastError());
        cleanupHandles();
        return Platform::TranslateError();
      }
      stdHandles[n] = handle;
      ownsStdHandle[n] = true;
      break;
    }

    case RedirectMode::buffer: {
      // Only output/error support buffer redirection.
      DS2ASSERT(n != Index(StreamIndex::input));
      SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
      HANDLE readEnd, writeEnd;
      if (!::CreatePipe(&readEnd, &writeEnd, &sa, 0)) {
        DS2LOG(Warning, "CreatePipe n=%zu failed err=%#lx", n,
               ::GetLastError());
        cleanupHandles();
        return Platform::TranslateError();
      }
      // Only the write end (handed to the child) should be inheritable;
      // the read end is ours alone and must never leak into any other
      // child ds2 spawns later.
      ::SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

      stdHandles[n] = writeEnd;
      ownsStdHandle[n] = true;
      parentPipeReadEnd[n] = readEnd;
      break;
    }
    }
  }

  size_t resolvedHandleCount = 0;
  for (size_t n = 0; n < kStreamCount; n++)
    if (stdHandles[n] != nullptr)
      ++resolvedHandleCount;

  if (resolvedHandleCount == 0)
    // Every requested stream resolved to "nothing to hand over" (e.g. an
    // explicit Console redirect was requested, but this process itself has
    // no console of its own to pass through). cleanupHandles() would be a
    // no-op here since Null/File/Buffer always either produce an owned
    // handle or return early on error above. There is nothing left to
    // restrict inheritance to, so fall back to an ordinary invocation.
    return launchPlain();

  // Restrict handle inheritance to exactly the (up to three) handles we are
  // handing to the child. Blanket bInheritHandles=TRUE would additionally
  // duplicate every other inheritable handle this ds2 process currently
  // holds (other clients' sockets, log files, ...) into the child, which is
  // both a resource leak and, for buffered streams specifically, would
  // duplicate our own pipe write end into the child, preventing us from
  // ever observing EOF on it.
  std::array<HANDLE, kStreamCount> handleList;
  DWORD handleCount = 0;
  for (size_t n = 0; n < kStreamCount; n++)
    if (stdHandles[n] != nullptr)
      handleList[handleCount++] = stdHandles[n];

  SIZE_T attributeListSize = 0;
  ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeListSize);

  std::vector<uint8_t> attributeListBuffer(attributeListSize);
  auto attributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
      attributeListBuffer.data());

  if (!::InitializeProcThreadAttributeList(attributeList, 1, 0,
                                           &attributeListSize)) {
    DS2LOG(Warning, "InitializeProcThreadAttributeList failed err=%#lx",
           ::GetLastError());
    cleanupHandles();
    return Platform::TranslateError();
  }

  if (!::UpdateProcThreadAttribute(
          attributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
          handleList.data(), handleCount * sizeof(HANDLE), nullptr, nullptr)) {
    DS2LOG(Warning, "UpdateProcThreadAttribute failed err=%#lx handleCount=%lu",
           ::GetLastError(), handleCount);
    ::DeleteProcThreadAttributeList(attributeList);
    cleanupHandles();
    return Platform::TranslateError();
  }

  STARTUPINFOEXW si = {};
  si.StartupInfo.cb = sizeof(si);
  si.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
  si.StartupInfo.hStdInput = stdHandles[Index(StreamIndex::input)];
  si.StartupInfo.hStdOutput = stdHandles[Index(StreamIndex::output)];
  si.StartupInfo.hStdError = stdHandles[Index(StreamIndex::error)];
  si.lpAttributeList = attributeList;

  BOOL result = ::CreateProcessW(
      nullptr, &commandLine[0], nullptr, nullptr, /*bInheritHandles=*/TRUE,
      creationFlags | EXTENDED_STARTUPINFO_PRESENT, environmentArg,
      workingDirectoryArg, &si.StartupInfo, &pi);
  if (!result)
    DS2LOG(Warning, "CreateProcessW failed err=%#lx cmdline='%ls'",
           ::GetLastError(), commandLine.c_str());

  ::DeleteProcThreadAttributeList(attributeList);

  // The child now has its own copy (if any) of every handle we handed it;
  // close ours so a peer reading a buffered pipe can observe EOF, and so we
  // don't otherwise leak these handles.
  for (size_t n = 0; n < kStreamCount; n++)
    if (ownsStdHandle[n] && stdHandles[n] != nullptr)
      ::CloseHandle(stdHandles[n]);

  if (!result) {
    for (size_t n = 0; n < kStreamCount; n++)
      if (parentPipeReadEnd[n] != nullptr)
        ::CloseHandle(parentPipeReadEnd[n]);
    return Platform::TranslateError();
  }

  _pid = pi.dwProcessId;
  _handle = pi.hProcess;
  _tid = pi.dwThreadId;
  _threadHandle = pi.hThread;

  for (size_t n = 0; n < kStreamCount; n++) {
    if (parentPipeReadEnd[n] == nullptr)
      continue;

    _descriptors[n].handle = parentPipeReadEnd[n];
    _redirectThreads[n] =
        std::thread(&ProcessSpawner::redirectionThread, this, n);
  }

  return kSuccess;
}

ErrorCode ProcessSpawner::wait() {
  if (_pid == 0)
    return kErrorProcessNotFound;

  DWORD rc = ::WaitForSingleObject(_handle, INFINITE);
  if (rc != WAIT_OBJECT_0)
    return Platform::TranslateError();

  DWORD exitCode = 0;
  if (!::GetExitCodeProcess(_handle, &exitCode))
    return Platform::TranslateError();

  for (auto &thread : _redirectThreads)
    if (thread.joinable())
      thread.join();

  ::CloseHandle(_threadHandle);
  ::CloseHandle(_handle);
  _handle = nullptr;
  _threadHandle = nullptr;
  _pid = 0;
  _exitStatus = static_cast<int>(exitCode);

  return kSuccess;
}

bool ProcessSpawner::isRunning() const {
  if (_pid == 0)
    return false;

  return ::WaitForSingleObject(_handle, 0) == WAIT_TIMEOUT;
}

//
// Redirector Thread
//
// One instance of this runs per RedirectMode::buffer stream (there can be up
// to two -- output and error -- draining concurrently into the shared
// _outputBuffer under a mutex).
//
void ProcessSpawner::redirectionThread(size_t index) {
  HANDLE handle = _descriptors[index].handle;

  for (;;) {
    char buf[256];
    DWORD nRead = 0;
    if (!::ReadFile(handle, buf, sizeof(buf), &nRead, nullptr) || nRead == 0)
      break;

    std::lock_guard<std::mutex> lock(_outputBufferMutex);
    _outputBuffer.insert(_outputBuffer.end(), &buf[0], &buf[nRead]);
  }

  ::CloseHandle(handle);
  _descriptors[index].handle = nullptr;
}

} // namespace Host
} // namespace ds2
