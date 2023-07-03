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

} // extern "C"
// clang-format on

#endif // !DOXYGEN

#endif
