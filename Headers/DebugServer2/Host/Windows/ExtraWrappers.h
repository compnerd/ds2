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

#include "DebugServer2/Base.h"

#include <cstdarg>
#include <cstdio>
#include <windows.h>

#define DS2_EXCEPTION_UNCAUGHT_COM 0x800706BA
#define DS2_EXCEPTION_UNCAUGHT_USER 0xE06D7363
#define DS2_EXCEPTION_UNCAUGHT_WINRT 0x40080201
#define DS2_EXCEPTION_VC_THREAD_NAME_SET 0x406D1388

#if !defined(HAVE_WaitForDebugEventEx)
#define WaitForDebugEventEx WaitForDebugEvent
#endif

// Some APIs are not exposed when building for UAP.
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

#if !defined(DOXYGEN)

// clang-format off
extern "C" {

#if !defined(UNLEN)
#define UNLEN 256
#endif

#if !defined(HAVE_GetWindowsDirectoryW)
WINBASEAPI UINT WINAPI GetWindowsDirectoryW(
  _Out_ LPWSTR lpBuffer,
  _In_  UINT   uSize
);
#endif

#if !defined(HAVE_GetModuleHandleA)
WINBASEAPI HMODULE WINAPI GetModuleHandleA(
  _In_opt_ LPCSTR lpModuleName
);
#endif

#if !defined(HAVE_GetModuleHandleW)
WINBASEAPI HMODULE WINAPI GetModuleHandleW(
  _In_opt_ LPCWSTR lpModuleName
);
#endif

#if !defined(HAVE_struct__STARTUPINFOW)
typedef struct _STARTUPINFOW {
    DWORD  cb;
    LPWSTR lpReserved;
    LPWSTR lpDesktop;
    LPWSTR lpTitle;
    DWORD  dwX;
    DWORD  dwY;
    DWORD  dwXSize;
    DWORD  dwYSize;
    DWORD  dwXCountChars;
    DWORD  dwYCountChars;
    DWORD  dwFillAttribute;
    DWORD  dwFlags;
    WORD   wShowWindow;
    WORD   cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOW, *LPSTARTUPINFOW;
#endif

#if !defined(HAVE_struct__PROCESS_INFORMATION)
typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
} PROCESS_INFORMATION, *LPPROCESS_INFORMATION;
#endif

#if !defined(HAVE_HANDLE_FLAG_INHERIT)
#define HANDLE_FLAG_INHERIT 0x00000001
#endif

// InitializeProcThreadAttributeList/UpdateProcThreadAttribute and the
// PROC_THREAD_ATTRIBUTE_LIST type are available in this partition already
// (they back ConPTY support); only the STARTUPINFOEXW struct and the
// PROC_THREAD_ATTRIBUTE_HANDLE_LIST attribute constant are missing.
#if !defined(HAVE_PROC_THREAD_ATTRIBUTE_HANDLE_LIST)
#if !defined(ProcThreadAttributeValue)
#define ProcThreadAttributeValue(Number, Thread, Input, Additive)            \
  ((Number) | ((Thread) ? 0x00010000 : 0) | ((Input) ? 0x00020000 : 0) |     \
   ((Additive) ? 0x00040000 : 0))
#endif
// ProcThreadAttributeHandleList == 2 in the PROC_THREAD_ATTRIBUTE_NUM enum.
#define PROC_THREAD_ATTRIBUTE_HANDLE_LIST                                   \
  ProcThreadAttributeValue(2, FALSE, TRUE, FALSE)
#endif

#if !defined(HAVE_STARTUPINFOEXW)
typedef struct _STARTUPINFOEXW {
    STARTUPINFOW StartupInfo;
    PPROC_THREAD_ATTRIBUTE_LIST lpAttributeList;
} STARTUPINFOEXW, *LPSTARTUPINFOEXW;
#endif

#if !defined(HAVE_STARTF_USESTDHANDLES)
#define STARTF_USESTDHANDLES 0x00000100
#endif

#if !defined(HAVE_CreateFileW)
WINBASEAPI HANDLE WINAPI CreateFileW(
  _In_     LPCWSTR               lpFileName,
  _In_     DWORD                 dwDesiredAccess,
  _In_     DWORD                 dwShareMode,
  _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  _In_     DWORD                 dwCreationDisposition,
  _In_     DWORD                 dwFlagsAndAttributes,
  _In_opt_ HANDLE                hTemplateFile
);
#endif

#if !defined(HAVE_TerminateThread)
WINBASEAPI BOOL WINAPI TerminateThread(
  _Inout_ HANDLE hThread,
  _In_    DWORD  dwExitCode
);
#endif

#if !defined(HAVE_SetThreadContext)
WINBASEAPI BOOL WINAPI SetThreadContext(
  _In_  HANDLE hThread,
  _In_  const CONTEXT *lpContext
);
#endif

#if !defined(HAVE_ReadProcessMemory)
WINBASEAPI BOOL WINAPI ReadProcessMemory(
  _In_   HANDLE hProcess,
  _In_   LPCVOID lpBaseAddress,
  _Out_  LPVOID lpBuffer,
  _In_   SIZE_T nSize,
  _Out_  SIZE_T *lpNumberOfBytesRead
);
#endif

#if !defined(HAVE_WriteProcessMemory)
WINBASEAPI BOOL WINAPI WriteProcessMemory(
  _In_   HANDLE hProcess,
  _In_   LPVOID lpBaseAddress,
  _In_   LPCVOID lpBuffer,
  _In_   SIZE_T nSize,
  _Out_  SIZE_T *lpNumberOfBytesWritten
);
#endif

#if !defined(HAVE_WaitForDebugEvent)
WINBASEAPI BOOL APIENTRY WaitForDebugEvent(
  _Out_ LPDEBUG_EVENT lpDebugEvent,
  _In_  DWORD         dwMilliseconds
);
#endif

#if !defined(HAVE_ContinueDebugEvent)
WINBASEAPI BOOL WINAPI ContinueDebugEvent(
  _In_  DWORD dwProcessId,
  _In_  DWORD dwThreadId,
  _In_  DWORD dwContinueStatus
);
#endif

#if !defined(HAVE_DebugActiveProcess)
WINBASEAPI BOOL WINAPI DebugActiveProcess(
  _In_  DWORD dwProcessId
);
#endif

#if !defined(HAVE_DebugActiveProcessStop)
WINBASEAPI BOOL WINAPI DebugActiveProcessStop(
  _In_ DWORD dwProcessId
);
#endif

#if !defined(HAVE_VirtualAllocEx)
WINBASEAPI LPVOID WINAPI VirtualAllocEx(
  _In_     HANDLE hProcess,
  _In_opt_ LPVOID lpAddress,
  _In_     SIZE_T dwSize,
  _In_     DWORD  flAllocationType,
  _In_     DWORD  flProtect
);
#endif

#if !defined(HAVE_VirtualFreeEx)
WINBASEAPI BOOL WINAPI VirtualFreeEx(
  _In_ HANDLE hProcess,
  _In_ LPVOID lpAddress,
  _In_ SIZE_T dwSize,
  _In_ DWORD  dwFreeType
);
#endif

#if !defined(HAVE_VirtualQueryEx)
WINBASEAPI SIZE_T WINAPI VirtualQueryEx(
  _In_     HANDLE                    hProcess,
  _In_opt_ LPCVOID                   lpAddress,
  _Out_    PMEMORY_BASIC_INFORMATION lpBuffer,
  _In_     SIZE_T                    dwLength
);
#endif

#if !defined(HAVE_EnumProcessModules)
// psapi.h's #define EnumProcessModules -> K32EnumProcessModules (PSAPI_VERSION
// > 1) is itself nested inside the same DESKTOP|SYSTEM|GAMES guard as the
// rest of the header, so it's absent here too, and callers reference the
// literal name "EnumProcessModules" directly, not the K32-prefixed one.
BOOL WINAPI EnumProcessModules(
  _In_                     HANDLE   hProcess,
  _Out_writes_bytes_(cb)   HMODULE *lphModule,
  _In_                     DWORD    cb,
  _Out_                    LPDWORD  lpcbNeeded
);
#endif

#if !defined(TH32CS_SNAPTHREAD)
#define TH32CS_SNAPTHREAD 0x00000004
#endif

#if !defined(HAVE_PTOP_LEVEL_EXCEPTION_FILTER)
typedef LONG (WINAPI *PTOP_LEVEL_EXCEPTION_FILTER)(
    _In_ struct _EXCEPTION_POINTERS *ExceptionInfo
    );
#endif

#if !defined(HAVE_LPTOP_LEVEL_EXCEPTION_FILTER)
typedef PTOP_LEVEL_EXCEPTION_FILTER LPTOP_LEVEL_EXCEPTION_FILTER;
#endif

// dbghelp.h's symbol handler: the whole header is unavailable outside the
// Desktop/WER/Games partitions, so its types need re-declaring here too, not
// just the functions that use them.
#if !defined(HAVE_SymInitialize) || !defined(HAVE_SymFromAddr) ||            \
    !defined(HAVE_SymGetLineFromAddr64)

#if !defined(MAX_SYM_NAME)
#define MAX_SYM_NAME 2000
#endif

typedef struct _SYMBOL_INFO {
  ULONG   SizeOfStruct;
  ULONG   TypeIndex;
  ULONG64 Reserved[2];
  ULONG   Index;
  ULONG   Size;
  ULONG64 ModBase;
  ULONG   Flags;
  ULONG64 Value;
  ULONG64 Address;
  ULONG   Register;
  ULONG   Scope;
  ULONG   Tag;
  ULONG   NameLen;
  ULONG   MaxNameLen;
  CHAR    Name[1];
} SYMBOL_INFO, *PSYMBOL_INFO;

typedef struct _IMAGEHLP_LINE64 {
  DWORD   SizeOfStruct;
  PVOID   Key;
  DWORD   LineNumber;
  PCHAR   FileName;
  DWORD64 Address;
} IMAGEHLP_LINE64, *PIMAGEHLP_LINE64;

#endif

#if !defined(HAVE_SymInitialize)
WINBASEAPI BOOL WINAPI SymInitialize(
  _In_     HANDLE hProcess,
  _In_opt_ PCSTR  UserSearchPath,
  _In_     BOOL   fInvadeProcess
);
#endif

#if !defined(HAVE_SymFromAddr)
WINBASEAPI BOOL WINAPI SymFromAddr(
  _In_      HANDLE       hProcess,
  _In_      DWORD64      Address,
  _Out_opt_ PDWORD64     Displacement,
  _Inout_   PSYMBOL_INFO Symbol
);
#endif

#if !defined(HAVE_SymGetLineFromAddr64)
WINBASEAPI BOOL WINAPI SymGetLineFromAddr64(
  _In_  HANDLE           hProcess,
  _In_  DWORD64          qwAddr,
  _Out_ PDWORD           pdwDisplacement,
  _Out_ PIMAGEHLP_LINE64 Line64
);
#endif

} // extern "C"
// clang-format on

#endif // !DOXYGEN

#endif
