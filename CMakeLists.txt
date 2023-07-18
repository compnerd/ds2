##
## Copyright (c) 2014-present, Facebook, Inc.
## All rights reserved.
##
## This source code is licensed under the University of Illinois/NCSA Open
## Source License found in the LICENSE file in the root directory of this
## source tree. An additional grant of patent rights can be found in the
## PATENTS file in the same directory.
##

cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0054 NEW)

project(DebugServer2
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SYSTEM_NAME MATCHES Windows)
  # NOTE(compnerd) disable exceptions in STL
  add_compile_definitions(_HAS_EXCEPTIONS=0)
endif()

set(OS_NAME ${CMAKE_SYSTEM_NAME})

string(STRIP "${OS_NAME}" OS_NAME)
if (OS_NAME MATCHES "WindowsStore")
  set(OS_NAME "Windows")
endif ()

if ("${OS_NAME}" MATCHES "Linux" OR "${OS_NAME}" MATCHES "Android")
  include(CheckIncludeFile)
  CHECK_INCLUDE_FILE(sys/personality.h HAVE_SYS_PERSONALITY_H)
  CHECK_INCLUDE_FILE(termios.h HAVE_TERMIOS_H)

  include(CheckSymbolExists)
  set(CMAKE_REQUIRED_DEFINITIONS -D_XOPEN_SOURCE=600;-D_GNU_SOURCE)
  CHECK_SYMBOL_EXISTS(posix_openpt "stdlib.h;fcntl.h" HAVE_POSIX_OPENPT)
  CHECK_SYMBOL_EXISTS(gettid "unistd.h" HAVE_GETTID)
  CHECK_SYMBOL_EXISTS(process_vm_readv "sys/uio.h" HAVE_PROCESS_VM_READV)
  CHECK_SYMBOL_EXISTS(process_vm_writev "sys/uio.h" HAVE_PROCESS_VM_WRITEV)
  CHECK_SYMBOL_EXISTS(tgkill "signal.h" HAVE_TGKILL)
  set(CMAKE_REQUIRED_DEFINITIONS)

  include(CheckTypeSize)
  set(CMAKE_EXTRA_INCLUDE_FILES unistd.h sys/ptrace.h)
  CHECK_TYPE_SIZE("enum __ptrace_request" ENUM_PTRACE_REQUEST)
  set(CMAKE_EXTRA_INCLUDE_FILES)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Global options (these apply for other subprojects like JSObjects) and must
  # be set before we include subdirectories.
  if (STATIC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
  endif ()

  if (SANITIZER)
    set(COMMON_FLAGS "${COMMON_FLAGS} -g -fno-omit-frame-pointer")
  endif ()

  if (SANITIZER STREQUAL "asan")
    set(COMMON_FLAGS "${COMMON_FLAGS} -fsanitize=address")
  elseif (SANITIZER STREQUAL "ubsan")
    set(COMMON_FLAGS "${COMMON_FLAGS} -fsanitize=integer -fsanitize=undefined")
  elseif (SANITIZER STREQUAL "tsan")
    set(COMMON_FLAGS "${COMMON_FLAGS} -fsanitize=thread")
  endif ()

  if (COVERAGE)
    set(COMMON_FLAGS "${COMMON_FLAGS} -g -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
  endif ()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")
endif ()

if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "ARM64" OR
   CMAKE_SYSTEM_PROCESSOR MATCHES "AArch64|ARM64")
  set(DS2_ARCHITECTURE ARM64)
elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "ARM" OR
      CMAKE_SYSTEM_PROCESSOR MATCHES "ARM")
  set(DS2_ARCHITECTURE ARM)
elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "X86" OR
      CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
  set(DS2_ARCHITECTURE X86)
elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "X64" OR
      CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
  set(DS2_ARCHITECTURE X86_64)
elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "RISCV32|RISCV64|RISCV128" OR
       CMAKE_SYSTEM_PROCESSOR MATCHES "riscv32|riscv64|riscv128")
  set(DS2_ARCHITECTURE RISCV)
else()
  message(SEND_ERROR "Unknown host architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# FIXME(compnerd) properly compute the executable suffix for the host.
# Currently, we are able to get away with this as we are not cross-compiling
# Windows, but this would be incorrect if we were.
get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(GENERATOR_IS_MULTI_CONFIG)
  set(RegsGen2_RELATIVE_PATH Release/regsgen2${CMAKE_EXECUTABLE_SUFFIX})
else()
  set(RegsGen2_RELATIVE_PATH regsgen2${CMAKE_EXECUTABLE_SUFFIX})
endif()

include(ExternalProject)
ExternalProject_Add(RegsGen2
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/Tools/RegsGen2
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/RegsGen2
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=Release
    -DCMAKE_SYSTEM_NAME:STRING=${CMAKE_HOST_SYSTEM_NAME}
    -DCMAKE_SYSTEM_PROCESSOR:STRING=${CMAKE_HOST_SYSTEM_PROCESSOR}
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS
    <BINARY_DIR>/${RegsGen2_RELATIVE_PATH})
ExternalProject_Get_Property(RegsGen2 BINARY_DIR)

add_executable(RegsGen2::RegsGen2 IMPORTED)
set_target_properties(RegsGen2::RegsGen2 PROPERTIES
  IMPORTED_LOCATION ${BINARY_DIR}/${RegsGen2_RELATIVE_PATH})
add_dependencies(RegsGen2::RegsGen2 RegsGen2)

# Generate different pointer size RISC-V definitions.
# FIXME(compnerd) this assumes all targets are RISCVnnnG, which may not be true.
foreach(WORDSIZE 32 64 128)
  configure_file(${PROJECT_SOURCE_DIR}/Definitions/RISCVnnn.json
    ${PROJECT_SOURCE_DIR}/Definitions/RISCV${WORDSIZE}.json @ONLY)
endforeach()

foreach(ARCH ARM ARM64 RISCV32 RISCV64 RISCV128 X86 X86_64)
  add_custom_command(OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/Headers/DebugServer2/Architecture/${ARCH}/RegistersDescriptors.h
    COMMAND
      $<TARGET_FILE:RegsGen2::RegsGen2>
        -a $<LOWER_CASE:${CMAKE_SYSTEM_NAME}>
        -I DebugServer2/Architecture/RegisterLayout.h
        -h -o ${CMAKE_CURRENT_BINARY_DIR}/Headers/DebugServer2/Architecture/${ARCH}/RegistersDescriptors.h
        -f "${PROJECT_SOURCE_DIR}/Definitions/${ARCH}.json"
    DEPENDS
      Definitions/${ARCH}.json
      RegsGen2::RegsGen2)

  add_custom_command(OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/Sources/Architecture/${ARCH}/RegistersDescriptors.cpp
    COMMAND
      $<TARGET_FILE:RegsGen2::RegsGen2>
        -a $<LOWER_CASE:${CMAKE_SYSTEM_NAME}>
        -I DebugServer2/Architecture/${ARCH}/RegistersDescriptors.h
        -c -o ${CMAKE_CURRENT_BINARY_DIR}/Sources/Architecture/${ARCH}/RegistersDescriptors.cpp
        -f "${PROJECT_SOURCE_DIR}/Definitions/${ARCH}.json"
    DEPENDS
      Definitions/${ARCH}.json
      RegsGen2::RegsGen2)
endforeach()

add_executable(ds2
  Sources/main.cpp

  Sources/Architecture/RegisterLayout.cpp

  Sources/Core/BreakpointManager.cpp
  Sources/Core/HardwareBreakpointManager.cpp
  Sources/Core/SoftwareBreakpointManager.cpp
  Sources/Core/CPUTypes.cpp
  Sources/Core/ErrorCodes.cpp
  Sources/Core/MessageQueue.cpp
  Sources/Core/SessionThread.cpp

  Sources/GDBRemote/DebugSessionImpl.cpp
  Sources/GDBRemote/DummySessionDelegateImpl.cpp
  Sources/GDBRemote/PacketProcessor.cpp
  Sources/GDBRemote/PlatformSessionImpl.cpp
  Sources/GDBRemote/ProtocolInterpreter.cpp
  Sources/GDBRemote/Session.cpp
  Sources/GDBRemote/SessionBase.cpp
  Sources/GDBRemote/SlaveSessionImpl.cpp
  Sources/GDBRemote/Structures.cpp

  Sources/Host/Common/Channel.cpp
  Sources/Host/Common/Platform.cpp
  Sources/Host/Common/QueueChannel.cpp
  Sources/Host/Common/Socket.cpp

  Sources/Target/Common/ProcessBase.cpp
  Sources/Target/Common/ThreadBase.cpp
  Sources/Target/Common/${DS2_ARCHITECTURE}/ProcessBase${DS2_ARCHITECTURE}.cpp

  Sources/Utils/Backtrace.cpp
  Sources/Utils/Log.cpp
  Sources/Utils/OptParse.cpp
  Sources/Utils/Stringify.cpp)

# Architecture Sources
if(DS2_ARCHITECTURE MATCHES "ARM|ARM64")
  target_sources(ds2 PRIVATE
    Sources/Architecture/ARM/ARMBranchInfo.cpp
    Sources/Architecture/ARM/SoftwareSingleStep.cpp
    Sources/Architecture/ARM/ThumbBranchInfo.cpp

    Sources/Core/ARM/HardwareBreakpointManager.cpp
    Sources/Core/ARM/SoftwareBreakpointManager.cpp)

  target_sources(ds2 PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/Headers/DebugServer2/Architecture/ARM/RegistersDescriptors.h
    ${CMAKE_CURRENT_BINARY_DIR}/Sources/Architecture/ARM/RegistersDescriptors.cpp)
  if(DS2_ARCHITECTURE MATCHES "ARM64")
    target_sources(ds2 PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/Headers/DebugServer2/Architecture/ARM64/RegistersDescriptors.h
      ${CMAKE_CURRENT_BINARY_DIR}/Sources/Architecture/ARM64/RegistersDescriptors.cpp)
  endif()
elseif(DS2_ARCHITECTURE MATCHES "X86|X86_64")
  target_sources(ds2 PRIVATE
    Sources/Core/X86/HardwareBreakpointManager.cpp
    Sources/Core/X86/SoftwareBreakpointManager.cpp)

  target_sources(ds2 PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/Headers/DebugServer2/Architecture/X86/RegistersDescriptors.h
    ${CMAKE_CURRENT_BINARY_DIR}/Sources/Architecture/X86/RegistersDescriptors.cpp)
  if(DS2_ARCHITECTURE MATCHES "X86_64")
    target_sources(ds2 PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/Headers/DebugServer2/Architecture/X86_64/RegistersDescriptors.h
      ${CMAKE_CURRENT_BINARY_DIR}/Sources/Architecture/X86_64/RegistersDescriptors.cpp)
    set_source_files_properties(
      ${CMAKE_CURRENT_BINARY_DIR}/Sources/Architecture/X86/RegistersDescriptors.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/Sources/Architecture/X86_64/RegistersDescriptors.cpp
      PROPERTIES
        COMPILE_OPTIONS $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-const-variable>)
  endif()
elseif(DS2_ARCHITECTURE STREQUAL RISCV)
  target_sources(ds2 PRIVATE
    Sources/Architecture/RISCV/SoftwareSingleStep.cpp
    Sources/Core/RISCV/HardwareBreakpointManager.cpp
    Sources/Core/RISCV/SoftwareBreakpointManager.cpp)

  if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "RISCV32" OR
       CMAKE_SYSTEM_PROCESSOR MATCHES "riscv32")
    target_sources(ds2 PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/Headers/DebugServer2/Architecture/RISCV32/RegistersDescriptors.h
      ${CMAKE_CURRENT_BINARY_DIR}/Sources/Architecture/RISCV32/RegistersDescriptors.cpp)
  elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "RISCV64" OR
       CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    target_sources(ds2 PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/Headers/DebugServer2/Architecture/RISCV64/RegistersDescriptors.h
      ${CMAKE_CURRENT_BINARY_DIR}/Sources/Architecture/RISCV64/RegistersDescriptors.cpp)
  elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "RISCV128" OR
       CMAKE_SYSTEM_PROCESSOR MATCHES "riscv128")
    target_sources(ds2 PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/Headers/DebugServer2/Architecture/RISCV128/RegistersDescriptors.h
      ${CMAKE_CURRENT_BINARY_DIR}/Sources/Architecture/RISCV128/RegistersDescriptors.cpp)
  endif()
endif()

# OS Sources
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  target_sources(ds2 PRIVATE
    Sources/Host/Windows/File.cpp
    Sources/Host/Windows/Platform.cpp
    Sources/Host/Windows/ProcessSpawner.cpp

    Sources/Target/Windows/Process.cpp
    Sources/Target/Windows/Thread.cpp
    Sources/Target/Windows/${DS2_ARCHITECTURE}/Thread${DS2_ARCHITECTURE}.cpp

    Sources/Utils/Windows/Daemon.cpp
    Sources/Utils/Windows/FaultHandler.cpp
    Sources/Utils/Windows/Stringify.cpp)
else()
  # Assumes that any non-Windows platform is POSIX.
  target_sources(ds2 PRIVATE
    Sources/Host/POSIX/File.cpp
    Sources/Host/POSIX/HandleChannel.cpp
    Sources/Host/POSIX/Platform.cpp
    Sources/Host/POSIX/ProcessSpawner.cpp
    Sources/Host/POSIX/PTrace.cpp

    Sources/Target/POSIX/Process.cpp
    Sources/Target/POSIX/Thread.cpp

    Sources/Utils/POSIX/Daemon.cpp
    Sources/Utils/POSIX/FaultHandler.cpp
    Sources/Utils/POSIX/Stringify.cpp)

  if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_sources(ds2 PRIVATE
      Sources/Target/Darwin/MachOProcess.cpp)
  else()
    # Assumes that any non-Darwin platform is ELF
    target_sources(ds2 PRIVATE
      Sources/Support/POSIX/ELFSupport.cpp

      Sources/Target/POSIX/ELFProcess.cpp)
  endif()

  if(CMAKE_SYSTEM_NAME MATCHES "Android|Linux")
    target_sources(ds2 PRIVATE
      Sources/Host/Linux/Platform.cpp
      Sources/Host/Linux/ProcFS.cpp
      Sources/Host/Linux/PTrace.cpp
      Sources/Host/Linux/${DS2_ARCHITECTURE}/PTrace${DS2_ARCHITECTURE}.cpp

      Sources/Target/Linux/Process.cpp
      Sources/Target/Linux/Thread.cpp
      Sources/Target/Linux/${DS2_ARCHITECTURE}/Process${DS2_ARCHITECTURE}.cpp)
  elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_sources(ds2 PRIVATE
      Sources/Host/Darwin/LibProc.cpp
      Sources/Host/Darwin/Platform.cpp
      Sources/Host/Darwin/PTrace.cpp
      Sources/Host/Darwin/Mach.cpp
      Sources/Host/Darwin/${DS2_ARCHITECTURE}/Mach${DS2_ARCHITECTURE}.cpp
      Sources/Host/Darwin/${DS2_ARCHITECTURE}/PTrace${DS2_ARCHITECTURE}.cpp

      Sources/Target/Darwin/Process.cpp
      Sources/Target/Darwin/Thread.cpp
      Sources/Target/Darwin/${DS2_ARCHITECTURE}/Process${DS2_ARCHITECTURE}.cpp
      Sources/Target/Darwin/${DS2_ARCHITECTURE}/Thread${DS2_ARCHITECTURE}.cpp)
  elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    target_sources(ds2 PRIVATE
      Sources/Host/FreeBSD/Platform.cpp
      Sources/Host/FreeBSD/ProcStat.cpp
      Sources/Host/FreeBSD/PTrace.cpp
      Sources/Host/FreeBSD/${DS2_ARCHITECTURE}/PTrace${DS2_ARCHITECTURE}.cpp

      Sources/Target/FreeBSD/Process.cpp
      Sources/Target/FreeBSD/Thread.cpp
      Sources/Target/FreeBSD/${DS2_ARCHITECTURE}/Process${DS2_ARCHITECTURE}.cpp)
  endif()
endif()

if(MSVC_IDE OR XCODE)
  file(GLOB_RECURSE DebugServer2_HEADERS Headers/DebugServer2/*.h)
  target_sources(ds2 PRIVATE ${DebugServer2_HEADERS})
endif()

target_include_directories(ds2 PUBLIC
  Headers
  ${CMAKE_CURRENT_BINARY_DIR}/Headers)

execute_process(COMMAND git rev-parse --short HEAD
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE DS2_GIT_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
set_property(SOURCE Sources/main.cpp APPEND PROPERTY COMPILE_DEFINITIONS
             DS2_GIT_HASH="${DS2_GIT_HASH}")

if ("${OS_NAME}" MATCHES "Windows")
  target_compile_definitions(ds2 PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX CINTERFACE)
  target_compile_definitions(ds2 PRIVATE WINVER=_WIN32_WINNT_WIN6
                                         _WIN32_WINNT=_WIN32_WINNT_WIN6)
endif ()

if ("${OS_NAME}" MATCHES "Linux" OR "${OS_NAME}" MATCHES "Android")
  foreach (CHECK SYS_PERSONALITY_H TERMIOS_H GETTID POSIX_OPENPT TGKILL
           PROCESS_VM_READV PROCESS_VM_WRITEV
           ENUM_PTRACE_REQUEST)
    if (HAVE_${CHECK})
      target_compile_definitions(ds2 PRIVATE HAVE_${CHECK})
    endif ()
  endforeach ()
elseif ("${OS_NAME}" MATCHES "Windows")
  set(CMAKE_EXTRA_INCLUDE_FILES windows.h)

  include(CheckFunctionExists)
  foreach (FUNC ContinueDebugEvent DebugActiveProcess DebugActiveProcessStop
      GetModuleHandleA GetModuleHandleW GetWindowsDirectoryW ReadProcessMemory
      SetThreadContext TerminateThread VirtualAllocEx VirtualFreeEx
      VirtualQueryEx WaitForDebugEventEx WriteProcessMemory)
    CHECK_FUNCTION_EXISTS(${FUNC} HAVE_${FUNC})
    if (HAVE_${CHECK})
      target_compile_definitions(ds2 PRIVATE HAVE_${FUNC})
    endif ()
  endforeach()

  include(CheckTypeSize)
  foreach (TYPE
      "struct _STARTUPINFOW" "struct _PROCESS_INFORMATION"
      PTOP_LEVEL_EXCEPTION_FILTER LPTOP_LEVEL_EXCEPTION_FILTER)
    string(REPLACE " " "_" SANITIZED_TYPE ${TYPE})
    CHECK_TYPE_SIZE(${TYPE} ${SANITIZED_TYPE})
    if (HAVE_${SANITIZED_TYPE})
      target_compile_definitions(ds2 PRIVATE HAVE_${SANITIZED_TYPE})
    endif ()
  endforeach ()

  set(CMAKE_EXTRA_INCLUDE_FILES)
endif ()

if (TIZEN)
  target_compile_definitions(ds2 PRIVATE __TIZEN__)
endif ()

add_subdirectory(Tools/JSObjects "${CMAKE_CURRENT_BINARY_DIR}/JSObjects")
target_link_libraries(ds2 PRIVATE jsobjects)

if ("${OS_NAME}" MATCHES "Android")
  target_link_libraries(ds2 PRIVATE log)
elseif ("${OS_NAME}" MATCHES "Linux" AND NOT TIZEN)
  target_link_libraries(ds2 PRIVATE dl)
endif ()

if ("${OS_NAME}" MATCHES "FreeBSD")
  target_link_libraries(ds2 PRIVATE util procstat)
endif ()

if ("${OS_NAME}" MATCHES "Windows")
  target_link_libraries(ds2 PRIVATE shlwapi ws2_32 dbghelp)
  if ("${CMAKE_LINKER}" MATCHES "lld")
    target_link_libraries(ds2 PRIVATE advapi32)
  endif()
  if ("${CMAKE_SYSTEM_NAME}" MATCHES "WindowsStore")
    target_link_libraries(ds2 PRIVATE onecore)
  else ()
    target_link_libraries(ds2 PRIVATE psapi)
  endif ()
endif ()

if (COVERAGE)
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_link_libraries(ds2 PRIVATE gcov)
  endif ()
endif ()

include(FindThreads)
if (STATIC AND DEFINED CMAKE_THREAD_LIBS_INIT AND
    (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
  target_link_options(ds2 PRIVATE
    -Wl,--whole-archive ${CMAKE_THREAD_LIBS_INIT} -Wl,--no-whole-archive)
else ()
  target_link_libraries(ds2 PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endif ()

install(TARGETS ds2 DESTINATION bin)
