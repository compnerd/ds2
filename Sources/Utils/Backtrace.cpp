//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Utils/Backtrace.h"
#include "DebugServer2/Utils/Log.h"

#if defined(PLATFORM_ANDROID) || defined(OS_DARWIN) || defined(__GLIBC__)
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#elif defined(OS_WIN32)
#include <dbghelp.h>
#include <windows.h>
#include <mutex>
#include <sstream>
#endif

#if defined(PLATFORM_ANDROID)
// Android provides libunwind in the NDK instead of glibc backtrace
#include <unwind.h>
#endif

namespace ds2 {
namespace Utils {

#if defined(PLATFORM_ANDROID)
struct unwind_state {
    void **buffer_cursor;
    void **buffer_end;
};

static _Unwind_Reason_Code UnwindTrace(struct _Unwind_Context* context,
                                       void* arg) {
  struct unwind_state* state = reinterpret_cast<struct unwind_state*>(arg);
  uintptr_t ip = _Unwind_GetIP(context);
  if (ip != 0) {
    if (state->buffer_cursor >= state->buffer_end)
      return _URC_END_OF_STACK; // Backtrace will be truncated

    *(state->buffer_cursor++) = reinterpret_cast<void*>(ip);
  }
  return _URC_NO_REASON;
}

// An approximation of glibc's backtrace using _Unwind_Backtrace
static int UnwindBacktrace(void *stack_buffer[], int stack_size) {
  unwind_state state = {
    .buffer_cursor = stack_buffer,
    .buffer_end = stack_buffer + stack_size,
  };

  _Unwind_Backtrace(UnwindTrace, &state);
  return state.buffer_cursor - stack_buffer;
}
#elif defined(OS_DARWIN) || (defined(__GLIBC__) && !defined(PLATFORM_TIZEN))
// Forward to glibc's backtrace
static int UnwindBacktrace(void *stack_buffer[], int stack_size) {
  return ::backtrace(stack_buffer, stack_size);
}
#endif

#if defined(PLATFORM_ANDROID) || defined(OS_DARWIN) || (defined(__GLIBC__) && !defined(PLATFORM_TIZEN))
static void PrintBacktraceEntrySimple(void *address) {
  DS2LOG(Error, "%" PRI_PTR, PRI_PTR_CAST(address));
}

void PrintBacktrace() {
  static const int kStackSize = 100;
  static void *stack[kStackSize];
  int stackEntries = UnwindBacktrace(stack, kStackSize);

  for (int i = 0; i < stackEntries; ++i) {
    Dl_info info;
    int res;

    res = ::dladdr(stack[i], &info);
    if (res < 0) {
      PrintBacktraceEntrySimple(stack[i]);
      continue;
    }

    char *demangled = nullptr;
    const char *name;
    if (info.dli_sname != nullptr) {
      demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &res);
      if (res == 0) {
        name = demangled;
      } else {
        name = info.dli_sname;
      }
    } else {
      name = "<unknown>";
    }

    DS2LOG(Error, "%" PRI_PTR " %s+%#" PRIxPTR " (%s+%#" PRIxPTR ")",
           PRI_PTR_CAST(stack[i]),
           name,
           static_cast<char *>(stack[i]) - static_cast<char *>(info.dli_saddr),
           info.dli_fname,
           static_cast<char *>(stack[i]) - static_cast<char *>(info.dli_fbase));

    ::free(demangled);
  }
}
#elif defined(OS_WIN32)

static HANDLE SymGetCurrentProcess() {
  // Based on
  // https://learn.microsoft.com/en-us/windows/win32/debug/initializing-the-symbol-handler
  HANDLE process = GetCurrentProcess();
  // Explanation for this is at
  // https://github.com/apple/swift/blob/0c56f1468c0f56ab9b96464ba0d63f7db4b84b69/stdlib/public/runtime/ImageInspectionCOFF.cpp#L42-L53.
  if (!DuplicateHandle(process, process, process, &process, 0, false,
                       DUPLICATE_SAME_ACCESS)) {
    DS2LOG(Error, "failed to dup current process handle; proceeding anyway");
  }
  if (SymInitialize(process, NULL, TRUE)) {
    return process;
  }
  // SymInitialize failed
  DS2LOG(Error, "SymInitialize returned error : %lu", GetLastError());
  return INVALID_HANDLE_VALUE;
}

static std::string SymbolForAddress(HANDLE currentProcess, void *pc) {
  uintptr_t address = reinterpret_cast<uintptr_t>(pc);
  // Based on
  // https://learn.microsoft.com/en-us/windows/win32/debug/retrieving-symbol-information-by-address
  char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
  PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(buffer);
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;
  DWORD64 offset = 0;
  if (!SymFromAddr(currentProcess, address, &offset, symbol)) {
    auto error = GetLastError();
    DS2LOG(Error, "SymFromAddr returned error : %lu", error);
    return "<unknown>";
  }
  std::ostringstream os;
  os << symbol->Name << " +0x" << std::hex << offset << std::dec;

  IMAGEHLP_LINE64 line = {};
  DWORD lineOffset = 0;
  line.SizeOfStruct = sizeof(line);
  if (SymGetLineFromAddr64(currentProcess, address, &lineOffset, &line)) {
    os << " (" << line.FileName << ":" << line.LineNumber << ")";
  } else {
    auto error = GetLastError();
    if (error == ERROR_INVALID_ADDRESS) {
      // This is not an error, it just means that the address is not in a
      // module that has line number information.
      DS2LOG(Debug, "SymGetLineFromAddr64 returned error: %lu", error);
    } else {
      DS2LOG(Error, "SymGetLineFromAddr64 returned error: %lu", error);
      os << " (<missing>)";
    }
  }
  return os.str();
}

void PrintBacktrace() {
  static std::mutex mutex;
  // DbgHelp methods are all single-threaded and require exclusion per
  // https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symgetlinefromaddr.
  std::unique_lock<std::mutex> lock(mutex);
  static HANDLE currentProcess = SymGetCurrentProcess();
  if (currentProcess == INVALID_HANDLE_VALUE) return;
  static const int kStackSize = 62;
  static PVOID stack[kStackSize];
  int stackEntries = CaptureStackBackTrace(0, kStackSize, stack, nullptr);

  for (int i = 0; i < stackEntries; ++i) {
    auto address = stack[i];
    DS2LOG(Info, "%" PRI_PTR " %s", PRI_PTR_CAST(address),
           SymbolForAddress(currentProcess, address).c_str());
  }
}
#else
void PrintBacktrace() { DS2LOG(Warning, "unable to print backtrace"); }
#endif
} // namespace Utils
} // namespace ds2
