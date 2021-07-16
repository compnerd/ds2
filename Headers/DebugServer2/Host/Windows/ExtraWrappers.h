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

#if !defined(GetVersionEx)
#define GetVersionEx GetVersionExW
#endif

#if !defined(HAVE_GetWindowsDirectoryW)
WINBASEAPI UINT WINAPI GetWindowsDirectoryW(
  _Out_ LPWSTR lpBuffer,
  _In_  UINT   uSize
);
#endif

#if !defined(GetWindowsDirectory)
#define GetWindowsDirectory GetWindowsDirectoryW
#endif

#if !defined(HAVE_EnumProcesses)
WINBASEAPI BOOL WINAPI K32EnumProcesses(
  _Out_ DWORD *pProcessIds,
  _In_  DWORD cb,
  _Out_ DWORD *pBytesReturned
);
#define EnumProcesses K32EnumProcesses
#endif

#if !defined(HAVE_K32EnumProcessModules)
WINBASEAPI BOOL WINAPI K32EnumProcessModules(
  _In_  HANDLE  hProcess,
  _Out_ HMODULE *lphModule,
  _In_  DWORD   cb,
  _Out_ LPDWORD lpcbNeeded
);
#endif

#if !defined(EnumProcessModules)
#define EnumProcessModules K32EnumProcessModules
#endif

#if !defined(HAVE_GetModuleBaseNameW)
WINBASEAPI DWORD WINAPI K32GetModuleBaseNameW(
  _In_     HANDLE  hProcess,
  _In_opt_ HMODULE hModule,
  _Out_    LPWSTR  lpBaseName,
  _In_     DWORD   nSize
);
#define GetModuleBaseNameW K32GetModuleBaseNameW
#endif

#if !defined(GetModuleBaseName)
#define GetModuleBaseName GetModuleBaseNameW
#endif

#if !defined(HAVE_GetModuleFileNameExW)
WINBASEAPI DWORD WINAPI K32GetModuleFileNameExW(
  _In_     HANDLE  hProcess,
  _In_opt_ HMODULE hModule,
  _Out_    LPWSTR  lpFilename,
  _In_     DWORD   nSize
);
#define GetModuleFileNameExW K32GetModuleFileNameExW
#endif

#if !defined(GetModuleFileNameEx)
#define GetModuleFileNameEx GetModuleFileNameExW
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

#if !defined(GetModuleHandle)
#define GetModuleHandle GetModuleHandleW
#endif

#if !defined(HAVE_OpenProcessToken)
WINBASEAPI BOOL WINAPI OpenProcessToken(
  _In_  HANDLE  ProcessHandle,
  _In_  DWORD   DesiredAccess,
  _Out_ PHANDLE TokenHandle
);
#endif

#if !defined(HAVE_GetTokenInformation)
WINBASEAPI BOOL WINAPI GetTokenInformation(
  _In_      HANDLE                  TokenHandle,
  _In_      TOKEN_INFORMATION_CLASS TokenInformationClass,
  _Out_opt_ LPVOID                  TokenInformation,
  _In_      DWORD                   TokenInformationLength,
  _Out_     PDWORD                  ReturnLength
);
#endif

#if !defined(HAVE_LookupAccountSidW)
WINBASEAPI BOOL WINAPI LookupAccountSidW(
  _In_opt_  LPCWSTR       lpSystemName,
  _In_      PSID          lpSid,
  _Out_opt_ LPWSTR        lpName,
  _Inout_   LPDWORD       cchName,
  _Out_opt_ LPWSTR        lpReferencedDomainName,
  _Inout_   LPDWORD       cchReferencedDomainName,
  _Out_     PSID_NAME_USE peUse
);
#endif

#if !defined(LookupAccountSid)
#define LookupAccountSid LookupAccountSidW
#endif

#if !defined(HAVE_CopySid)
WINBASEAPI BOOL WINAPI CopySid(
  _In_  DWORD nDestinationSidLength,
  _Out_ PSID  pDestinationSid,
  _In_  PSID  pSourceSid
);
#endif

#if !defined(HAVE_GetLengthSid)
WINBASEAPI DWORD WINAPI GetLengthSid(
  _In_ PSID pSid
);
#endif

#if !defined(HAVE_GetEnvironmentStringsW)
WINBASEAPI LPWCH WINAPI GetEnvironmentStringsW(void);
#endif

#if !defined(HAVE_FreeEnvironmentStringsW)
WINBASEAPI BOOL WINAPI FreeEnvironmentStringsW(
  _In_  LPWCH lpszEnvironmentBlock
);
#endif

#if !defined(HAVE_LPPROC_THREAD_ATTRIBUTE_LIST)
typedef struct PROC_THREAD_ATTRIBUTE_LIST *LPPROC_THREAD_ATTRIBUTE_LIST;
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

#if !defined(HAVE_struct__STARTUPINFOEXW)
typedef struct _STARTUPINFOEXW {
    STARTUPINFOW                 StartupInfo;
    LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList;
} STARTUPINFOEXW, *LPSTARTUPINFOEXW;
#endif

#if !defined(HAVE_struct__STARTUPINFOA)
typedef struct _STARTUPINFOA {
    DWORD  cb;
    LPSTR lpReserved;
    LPSTR lpDesktop;
    LPSTR lpTitle;
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
} STARTUPINFOA, *LPSTARTUPINFOA;
#endif

#if !defined(HAVE_struct__PROCESS_INFORMATION)
typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
} PROCESS_INFORMATION, *LPPROCESS_INFORMATION;
#endif

#if !defined(HAVE_CreateProcessW)
WINBASEAPI BOOL WINAPI CreateProcessW(
  _In_opt_     LPCWSTR lpApplicationName,
  _Inout_opt_  LPWSTR lpCommandLine,
  _In_opt_     LPSECURITY_ATTRIBUTES lpProcessAttributes,
  _In_opt_     LPSECURITY_ATTRIBUTES lpThreadAttributes,
  _In_         BOOL bInheritHandles,
  _In_         DWORD dwCreationFlags,
  _In_opt_     LPVOID lpEnvironment,
  _In_opt_     LPCWSTR lpCurrentDirectory,
  _In_         LPSTARTUPINFOW lpStartupInfo,
  _Out_        LPPROCESS_INFORMATION lpProcessInformation
);
#endif

#if !defined(CreateProcess)
#define CreateProcess CreateProcessW
#endif

#if !defined(HAVE_CreateThread)
WINBASEAPI HANDLE WINAPI CreateThread(
  _In_opt_   LPSECURITY_ATTRIBUTES lpThreadAttributes,
  _In_       SIZE_T dwStackSize,
  _In_       LPTHREAD_START_ROUTINE lpStartAddress,
  _In_opt_   LPVOID lpParameter,
  _In_       DWORD dwCreationFlags,
  _Out_opt_  LPDWORD lpThreadId
);
#endif

#if !defined(HAVE_ResumeThread)
WINBASEAPI DWORD WINAPI ResumeThread(
  _In_  HANDLE hThread
);
#endif

#if !defined(HAVE_TerminateThread)
WINBASEAPI BOOL WINAPI TerminateThread(
  _Inout_ HANDLE hThread,
  _In_    DWORD  dwExitCode
);
#endif

#if !defined(HAVE_SuspendThread)
WINBASEAPI DWORD WINAPI SuspendThread(
  _In_ HANDLE hThread
);
#endif

#if !defined(HAVE_GetThreadContext)
WINBASEAPI BOOL WINAPI GetThreadContext(
  _In_     HANDLE hThread,
  _Inout_  LPCONTEXT lpContext
);
#endif

#if !defined(HAVE_SetThreadContext)
WINBASEAPI BOOL WINAPI SetThreadContext(
  _In_  HANDLE hThread,
  _In_  const CONTEXT *lpContext
);
#endif

#if !defined(HAVE_GetThreadId)
WINBASEAPI DWORD WINAPI GetThreadId(
  _In_  HANDLE Thread
);
#endif

#if !defined(HAVE_OpenProcess)
WINBASEAPI HANDLE WINAPI OpenProcess(
  _In_  DWORD dwDesiredAccess,
  _In_  BOOL bInheritHandle,
  _In_  DWORD dwProcessId
);
#endif

#if !defined(HAVE_GetExitCodeProcess)
WINBASEAPI BOOL WINAPI GetExitCodeProcess(
  _In_  HANDLE  hProcess,
  _Out_ LPDWORD lpExitCode
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

#if !defined(HAVE_FlushInstructionCache)
WINBASEAPI BOOL WINAPI FlushInstructionCache(
  _In_ HANDLE  hProcess,
  _In_ LPCVOID lpBaseAddress,
  _In_ SIZE_T  dwSize
);
#endif

#if !defined(HAVE_ContinueDebugEvent)
WINBASEAPI BOOL WINAPI ContinueDebugEvent(
  _In_  DWORD dwProcessId,
  _In_  DWORD dwThreadId,
  _In_  DWORD dwContinueStatus
);
#endif

#if !defined(HAVE_TerminateProcess)
WINBASEAPI BOOL WINAPI TerminateProcess(
  _In_  HANDLE hProcess,
  _In_  UINT uExitCode
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

#if !defined(HAVE_struct_tagTHREADENTRY32)
typedef struct tagTHREADENTRY32 {
  DWORD dwSize;
  DWORD cntUsage;
  DWORD th32ThreadID;
  DWORD th32OwnerProcessID;
  LONG  tpBasePri;
  LONG  tpDeltaPri;
  DWORD dwFlags;
} THREADENTRY32, *PTHREADENTRY32, *LPTHREADENTRY32;
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

#if !defined(HAVE_SetUnhandledExceptionFilter)
WINBASEAPI LPTOP_LEVEL_EXCEPTION_FILTER WINAPI SetUnhandledExceptionFilter(
  _In_ LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter
);
#endif

} // extern "C"
// clang-format on

#endif // !DOXYGEN

#endif
