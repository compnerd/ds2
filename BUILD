# Copyright 2022 Saleem Abdulrasool <compnerd@compnerd.org>.  All Rights Reserved.
# SPDX-License-Identifier: BSD-3-Clause

load("@bazel_skylib//lib:selects.bzl", "selects")

package(default_visibility = ["//visibility:private"])

config_setting(
    name = "android-arm64",
    constraint_values = [
        "@platforms//os:android",
        "@platforms//cpu:arm64",
    ],
)

config_setting(
    name = "android-armv7",
    constraint_values = [
        "@platforms//os:android",
        "@platforms//cpu:armv7",
    ],
)

config_setting(
    name = "android-i686",
    constraint_values = [
        "@platforms//os:android",
        "@platforms//cpu:i386",
    ],
)

config_setting(
    name = "android-x86_64",
    constraint_values = [
        "@platforms//os:android",
        "@platforms//cpu:x86_64",
    ],
)

config_setting(
    name = "darwin-arm64",
    constraint_values = [
        "@platforms//os:macos",
        "@platforms//cpu:arm64",
    ]
)

config_setting(
    name = "darwin-x86_64",
    constraint_values = [
        "@platforms//os:macos",
        "@platforms//cpu:x86_64",
    ],
)

config_setting(
    name = "freebsd-x86_64",
    constraint_values = [
        "@platforms//os:freebsd",
        "@platforms//cpu:x86_64",
    ],
)

config_setting(
    name = "linux-arm64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:arm64",
    ],
)

config_setting(
    name = "linux-armv7",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:armv7",
    ],
)

config_setting(
    name = "linux-i686",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:i386",
    ],
)

config_setting(
    name = "linux-x86_64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
)

config_setting(
    name = "windows-armv7",
    constraint_values = [
        "@platforms//os:windows",
        "@platforms//cpu:armv7",
    ],
)

config_setting(
    name = "windows-i686",
    constraint_values = [
        "@platforms//os:windows",
        "@platforms//cpu:i386",
    ],
)

config_setting(
    name = "windows-x86_64",
    constraint_values = [
        "@platforms//os:windows",
        "@platforms//cpu:x86_64",
    ],
)

[
    genrule(
        name = "generated_riscv{}_definitions".format(wordsize),
        srcs = ["Definitions/RISCVnnn.json"],
        outs = ["Definitions/RISCV{}.json".format(wordsize)],
        cmd = "sed -e s,@WORDSIZE@,{},g $< > $@".format(wordsize),
    )
    for wordsize in (32, 64, 128)
]

[
    genrule(
        name = "generated_{}_register_definitions_header".format(arch),
        srcs = ["Definitions/{}.json".format(arch)],
        outs = ["Headers/DebugServer2/Architecture/{}/RegistersDescriptors.h".format(arch)],
        cmd = "$(location //Tools/RegsGen2:regsgen2) -a " + select({
            "@platforms//os:android": "linux",
            "@platforms//os:linux": "linux",
            "@platforms//os:freebsd": "freebsd",
            "@platforms//os:macos": "darwin",
            "@platforms//os:windows": "windows",
        }) + " -I DebugServer2/Architecture/RegisterLayout.h -h -o $@ -f $<",
        tools = ["//Tools/RegsGen2:regsgen2"],
    )
    for arch in ("ARM", "ARM64", "RISCV32", "RISCV64", "RISCV128", "X86", "X86_64")
]

[
    genrule(
        name = "generated_{}_register_definitions_source".format(arch),
        srcs = ["Definitions/{}.json".format(arch)],
        outs = ["Sources/Architecture/{}/RegistersDescriptors.cpp".format(arch)],
        cmd = "$(location //Tools/RegsGen2:regsgen2) -a " + select({
            "@platforms//os:android": "linux",
            "@platforms//os:linux": "linux",
            "@platforms//os:freebsd": "freebsd",
            "@platforms//os:macos": "darwin",
            "@platforms//os:windows": "windows",
        }) + " -I DebugServer2/Architecture/{}/RegistersDescriptors.h -c -o $@ -f $<".format(arch),
        tools = ["//Tools/RegsGen2:regsgen2"],
    )
    for arch in ("ARM", "ARM64", "RISCV32", "RISCV64", "RISCV128", "X86", "X86_64")
]

filegroup(
    name = "headers",
    srcs = [
        "Headers/DebugServer2/Base.h",
        "Headers/DebugServer2/Constants.h",
        "Headers/DebugServer2/Types.h",
        "Headers/DebugServer2/Architecture/CPUState.h",
        "Headers/DebugServer2/Architecture/Registers.h",
        "Headers/DebugServer2/Architecture/RegistersDescriptors.h",
        "Headers/DebugServer2/Architecture/RegisterLayout.h",
        "Headers/DebugServer2/Core/BreakpointManager.h",
        "Headers/DebugServer2/Core/CPUTypes.h",
        "Headers/DebugServer2/Core/ErrorCodes.h",
        "Headers/DebugServer2/Core/HardwareBreakpointManager.h",
        "Headers/DebugServer2/Core/MessageQueue.h",
        "Headers/DebugServer2/Core/SessionThread.h",
        "Headers/DebugServer2/Core/SoftwareBreakpointManager.h",
        "Headers/DebugServer2/GDBRemote/Mixins/FileOperationsMixin.h",
        "Headers/DebugServer2/GDBRemote/Mixins/ProcessLaunchMixin.h",
        "Headers/DebugServer2/GDBRemote/Base.h",
        "Headers/DebugServer2/GDBRemote/DebugSessionImpl.h",
        "Headers/DebugServer2/GDBRemote/DummySessionDelegateImpl.h",
        "Headers/DebugServer2/GDBRemote/PacketProcessor.h",
        "Headers/DebugServer2/GDBRemote/PlatformSessionImpl.h",
        "Headers/DebugServer2/GDBRemote/ProtocolHelpers.h",
        "Headers/DebugServer2/GDBRemote/ProtocolInterpreter.h",
        "Headers/DebugServer2/GDBRemote/Session.h",
        "Headers/DebugServer2/GDBRemote/SessionBase.h",
        "Headers/DebugServer2/GDBRemote/SessionDelegate.h",
        "Headers/DebugServer2/GDBRemote/Types.h",
        "Headers/DebugServer2/Host/Channel.h",
        "Headers/DebugServer2/Host/File.h",
        "Headers/DebugServer2/Host/Platform.h",
        "Headers/DebugServer2/Host/ProcessSpawner.h",
        "Headers/DebugServer2/Host/QueueChannel.h",
        "Headers/DebugServer2/Host/Socket.h",
        "Headers/DebugServer2/Target/Process.h",
        "Headers/DebugServer2/Target/ProcessBase.h",
        "Headers/DebugServer2/Target/ProcessDecl.h",
        "Headers/DebugServer2/Target/Thread.h",
        "Headers/DebugServer2/Target/ThreadBase.h",
        "Headers/DebugServer2/Utils/Backtrace.h",
        "Headers/DebugServer2/Utils/Bits.h",
        "Headers/DebugServer2/Utils/CompilerSupport.h",
        "Headers/DebugServer2/Utils/Daemon.h",
        "Headers/DebugServer2/Utils/Enums.h",
        "Headers/DebugServer2/Utils/HexValues.h",
        "Headers/DebugServer2/Utils/Log.h",
        "Headers/DebugServer2/Utils/MPL.h",
        "Headers/DebugServer2/Utils/OptParse.h",
        "Headers/DebugServer2/Utils/ScopedJanitor.h",
        "Headers/DebugServer2/Utils/String.h",
        "Headers/DebugServer2/Utils/Stringify.h",
        "Headers/DebugServer2/Utils/SwapEndian.h",
    ] + select({
        "@platforms//cpu:armv7": [
            "Headers/DebugServer2/Architecture/ARM/Branching.h",
            "Headers/DebugServer2/Architecture/ARM/CPUState.h",
            "Headers/DebugServer2/Architecture/ARM/SoftwareSingleStep.h",
            ":generated_ARM_register_definitions_header",
        ],
        "@platforms//cpu:arm64": [
            "Headers/DebugServer2/Architecture/ARM/Branching.h",
            "Headers/DebugServer2/Architecture/ARM/CPUState.h",
            "Headers/DebugServer2/Architecture/ARM/SoftwareSingleStep.h",
            "Headers/DebugServer2/Architecture/ARM64/CPUState.h",
            ":generated_ARM_reigster_definitions_header",
            ":generated_ARM64_register_definitions_header",
        ],
        "@platforms//cpu:i386": [
            "Headers/DebugServer2/Architecture/X86/CPUState.h",
            "Headers/DebugServer2/Architecture/X86/RegisterCopy.h",
            ":generated_X86_register_definitions_header",
        ],
        "@platforms//cpu:x86_64": [
            "Headers/DebugServer2/Architecture/X86/CPUState.h",
            "Headers/DebugServer2/Architecture/X86/RegisterCopy.h",
            "Headers/DebugServer2/Architecture/X86_64/CPUState.h",
            ":generated_X86_register_definitions_header",
            ":generated_X86_64_register_definitions_header",
        ],
        "@platforms//cpu:riscv32": [
            "Headers/DebugServer2/Architecture/RISCV/CPUState.h",
            "Headers/DebugServer2/Architecture/RISCV/SoftwareSingleStep.h",
            ":generated_RISCV32_register_definitions_header",
        ],
        "@platforms//cpu:riscv64": [
            "Headers/DebugServer2/Architecture/RISCV/CPUState.h",
            "Headers/DebugServer2/Architecture/RISCV/SoftwareSingleStep.h",
            ":generated_RISCV64_register_definitions_header",
        ],
    }) + selects.with_or({
        "@platforms//os:windows": [
            "Headers/DebugServer2/Host/Windows/ProcessSpawner.h",
            "Headers/DebugServer2/Target/Windows/Process.h",
            "Headers/DebugServer2/Target/Windows/Thread.h",
        ],
        ("@platforms//os:android", "@platforms//os:freebsd", "@platforms//os:linux", "@platforms//os:macos"): [
            "Headers/DebugServer2/Host/POSIX/HandleChannel.h",
            "Headers/DebugServer2/Host/POSIX/PTrace.h",
            "Headers/DebugServer2/Host/POSIX/ProcessSpawner.h",
            "Headers/DebugServer2/Target/POSIX/Process.h",
            "Headers/DebugServer2/Target/POSIX/Thread.h",
        ],
    }) + selects.with_or({
        "@platforms//os:windows": [],
        "@platforms//os:macos": [
            "Headers/DebugServer2/Target/Darwin/MachOProcess.h",
        ],
        ("@platforms//os:android", "@platforms//os:freebsd", "@platforms//os:linux"): [
            "Headers/DebugServer2/Support/POSIX/ELFSupport.h",
            "Headers/DebugServer2/Target/POSIX/ELFProcess.h",
        ],
    }) + selects.with_or({
        ("@platforms//os:android", "@platforms//os:linux"): [
            "Headers/DebugServer2/Host/Linux/ExtraWrappers.h",
            "Headers/DebugServer2/Host/Linux/ProcFS.h",
            "Headers/DebugServer2/Host/Linux/PTrace.h",
            "Headers/DebugServer2/Target/Linux/Process.h",
            "Headers/DebugServer2/Target/Linux/Thread.h",
        ],
        "@platforms//os:freebsd": [
            "Headers/DebugServer2/Host/FreeBSD/ProcStat.h",
            "Headers/DebugServer2/Host/FreeBSD/PTrace.h",
            "Headers/DebugServer2/Target/FreeBSD/Process.h",
            "Headers/DebugServer2/Target/FreeBSD/Thread.h",
        ],
        "@platforms//os:macos": [
            "Headers/DebugServer2/Host/Darwin/LibProc.h",
            "Headers/DebugServer2/Host/Darwin/Mach.h",
            "Headers/DebugServer2/Host/Darwin/PTrace.h",
            "Headers/DebugServer2/Target/Darwin/Process.h",
            "Headers/DebugServer2/Target/Darwin/Thread.h",
        ],
        "@platforms//os:windows": [
            "Headers/DebugServer2/Host/Windows/ExtraWrappers.h",
        ],
    }) + selects.with_or({
        (":android-armv7", ":linux-armv7"): [
            "Headers/DebugServer2/Host/Linux/ARM/HwCaps.h",
        ],
        (":android-arm64", ":linux-arm64"): [
            "Headers/DebugServer2/Host/Linux/ARM/HwCaps.h",
            "Headers/DebugServer2/Host/Linux/ARM64/Syscalls.h",
        ],
        (":android-i686", ":linux-i686"): [
            "Headers/DebugServer2/Host/Linux/X86/Syscalls.h",
        ],
        (":android-x86_64", ":linux-x86_64"): [
            "Headers/DebugServer2/Host/Linux/X86/Syscalls.h",
            "Headers/DebugServer2/Host/Linux/X86_64/Syscalls.h",
        ],
        "//conditions:default": [],
    }),
)

filegroup(
    name = "sources",
    srcs = [
        "Sources/main.cpp",
        "Sources/Architecture/RegisterLayout.cpp",
        "Sources/Core/BreakpointManager.cpp",
        "Sources/Core/HardwareBreakpointManager.cpp",
        "Sources/Core/SoftwareBreakpointManager.cpp",
        "Sources/Core/CPUTypes.cpp",
        "Sources/Core/ErrorCodes.cpp",
        "Sources/Core/MessageQueue.cpp",
        "Sources/Core/SessionThread.cpp",
        "Sources/GDBRemote/Mixins/FileOperationsMixin.hpp",
        "Sources/GDBRemote/Mixins/ProcessLaunchMixin.hpp",
        "Sources/GDBRemote/DebugSessionImpl.cpp",
        "Sources/GDBRemote/DummySessionDelegateImpl.cpp",
        "Sources/GDBRemote/PacketProcessor.cpp",
        "Sources/GDBRemote/PlatformSessionImpl.cpp",
        "Sources/GDBRemote/ProtocolInterpreter.cpp",
        "Sources/GDBRemote/Session.cpp",
        "Sources/GDBRemote/SessionBase.cpp",
        "Sources/GDBRemote/Structures.cpp",
        "Sources/Host/Common/Channel.cpp",
        "Sources/Host/Common/Platform.cpp",
        "Sources/Host/Common/QueueChannel.cpp",
        "Sources/Host/Common/Socket.cpp",
        "Sources/Target/Common/ProcessBase.cpp",
        "Sources/Target/Common/ThreadBase.cpp",
        "Sources/Utils/Backtrace.cpp",
        "Sources/Utils/Log.cpp",
        "Sources/Utils/OptParse.cpp",
        "Sources/Utils/Stringify.cpp",
    ] + select({
        "@platforms//cpu:arm64": [
            ":generated_ARM_register_definitions_source",
            ":generated_ARM64_register_definitions_source",
            "Sources/Architecture/ARM/ARMBranchInfo.cpp",
            "Sources/Architecture/ARM/SoftwareSingleStep.cpp",
            "Sources/Architecture/ARM/ThumbBranchInfo.cpp",
            "Sources/Core/ARM/HardwareBreakpointManager.cpp",
            "Sources/Core/ARM/SoftwareBreakpointManager.cpp",
            "Sources/Target/Common/ARM64/ProcessBaseARM64.cpp",
        ],
        "@platforms//cpu:armv7": [
            ":generated_ARM_register_definitions_source",
            "Sources/Architecture/ARM/ARMBranchInfo.cpp",
            "Sources/Architecture/ARM/SoftwareSingleStep.cpp",
            "Sources/Architecture/ARM/ThumbBranchInfo.cpp",
            "Sources/Core/ARM/HardwareBreakpointManager.cpp",
            "Sources/Core/ARM/SoftwareBreakpointManager.cpp",
            "Sources/Target/Common/ARM/ProcessBaseARM.cpp",
        ],
        "@platforms//cpu:i386": [
            ":generated_X86_register_definitions_source",
            "Sources/Core/X86/HardwareBreakpointManager.cpp",
            "Sources/Core/X86/SoftwareBreakpointManager.cpp",
            "Sources/Target/Common/X86/ProcessBaseX86.cpp",
        ],
        "@platforms//cpu:riscv32": [
            ":generated_RISCV32_register_definitions_source",
            "Sources/Architecture/RISCV/SoftwareSingleStep.cpp",
            "Sources/Core/RISCV/HardwareBreakpointManager.cpp",
            "Sources/Core/RISCV/SoftwareBreakpointManager.cpp",
            "Sources/Target/Common/RISCV/ProcessBaseRISCV.cpp",
        ],
        "@platforms//cpu:riscv64": [
            ":generated_RISCV64_register_definitions_source",
            "Sources/Architecture/RISCV/SoftwareSingleStep.cpp",
            "Sources/Core/RISCV/HardwareBreakpointManager.cpp",
            "Sources/Core/RISCV/SoftwareBreakpointManager.cpp",
            "Sources/Target/Common/RISCV/ProcessBaseRISCV.cpp",
        ],
        "@platforms//cpu:x86_64": [
            ":generated_X86_register_definitions_source",
            ":generated_X86_64_register_definitions_source",
            "Sources/Core/X86/HardwareBreakpointManager.cpp",
            "Sources/Core/X86/SoftwareBreakpointManager.cpp",
            "Sources/Target/Common/X86_64/ProcessBaseX86_64.cpp",
        ],
    }) + selects.with_or({
        "@platforms//os:windows": [
            "Sources/Host/Windows/File.cpp",
            "Sources/Host/Windows/Platform.cpp",
            "Sources/Host/Windows/ProcessSpawner.cpp",
            "Sources/Target/Windows/Process.cpp",
            "Sources/Target/Windows/Thread.cpp",
            "Sources/Utils/Windows/Daemon.cpp",
            "Sources/Utils/Windows/FaultHandler.cpp",
            "Sources/Utils/Windows/Stringify.cpp",
        ],
        ("@platforms//os:android", "@platforms//os:freebsd", "@platforms//os:linux", "@platforms//os:macos"): [
            "Sources/Host/POSIX/File.cpp",
            "Sources/Host/POSIX/HandleChannel.cpp",
            "Sources/Host/POSIX/Platform.cpp",
            "Sources/Host/POSIX/ProcessSpawner.cpp",
            "Sources/Host/POSIX/PTrace.cpp",
            "Sources/Target/POSIX/Process.cpp",
            "Sources/Target/POSIX/Thread.cpp",
            "Sources/Utils/POSIX/Daemon.cpp",
            "Sources/Utils/POSIX/FaultHandler.cpp",
            "Sources/Utils/POSIX/Stringify.cpp",
        ],
    }) + selects.with_or({
        "@platforms//os:windows": [],
        "@platforms//os:macos": [
            "Sources/Target/Darwin/MachOProcess.cpp",
        ],
        ("@platforms//os:android", "@platforms//os:freebsd", "@platforms//os:linux"): [
            "Sources/Support/POSIX/ELFSupport.cpp",
            "Sources/Target/POSIX/ELFProcess.cpp",
        ],
    }) + selects.with_or({
        ("@platforms//os:android", "@platforms//os:linux"): [
            "Sources/Host/Linux/Platform.cpp",
            "Sources/Host/Linux/ProcFS.cpp",
            "Sources/Host/Linux/PTrace.cpp",
            "Sources/Target/Linux/Process.cpp",
            "Sources/Target/Linux/Thread.cpp",
        ],
        "@platforms//os:freebsd": [
            "Sources/Host/FreeBSD/Platform.cpp",
            "Sources/Host/FreeBSD/ProcStat.cpp",
            "Sources/Host/FreeBSD/PTrace.cpp",
            "Sources/Target/FreeBSD/Process.cpp",
            "Sources/Target/FreeBSD/Thread.cpp",
        ],
        "@platforms//os:macos": [
            "Sources/Host/Darwin/LibProc.cpp",
            "Sources/Host/Darwin/Platform.cpp",
            "Sources/Host/Darwin/PTrace.cpp",
            "Sources/Host/Darwin/Mach.cpp",
            "Sources/Target/Darwin/Process.cpp",
            "Sources/Target/Darwin/Thread.cpp",
        ],
        "@platforms//os:windows": [
        ],
    }) + selects.with_or({
        (":android-arm64", ":linux-arm64"): [
            "Sources/Host/Linux/ARM64/PTraceARM64.cpp",
            "Sources/Target/Linux/ARM64/ProcessAR64.cpp",
        ],
        (":android-armv7", ":linux-armv7"): [
            "Sources/Host/Linux/ARM/PTraceARM.cpp",
            "Sources/Target/Linux/ARM/ProcessARM.cpp",
        ],
        (":android-i686", ":linux-i686"): [
            "Sources/Host/Linux/X86/PTraceX86.cpp",
            "Sources/Target/Linux/X86/ProcessX86.cpp",
        ],
        (":android-x86_64", ":linux-x86_64"): [
            "Sources/Host/Linux/X86_64/PTraceX86_64.cpp",
            "Sources/Target/Linux/X86_64/ProcessX86_64.cpp",
        ],
        ":darwin-arm64": [
            "Sources/Host/Darwin/ARM64/MachARM64.cpp",
            "Sources/Host/Darwin/ARM64/PTraceARM64.cpp",
            "Sources/Target/Darwin/ARM64/ProcessARM64.cpp",
            "Sources/Target/Darwin/ARM64/ThreadARM64.cpp",
        ],
        ":darwin-x86_64": [
            "Sources/Host/Darwin/X86_64/MachX86_64.cpp",
            "Sources/Host/Darwin/X86_64/PTraceX86_64.cpp",
            "Sources/Target/Darwin/X86_64/ProcessX86_64.cpp",
            "Sources/Target/Darwin/X86_64/ThreadX86_64.cpp",
        ],
        ":freebsd-x86_64": [
            "Sources/Host/FreeBSD/X86_64/PTraceX86_64.cpp",
            "Sources/Target/FreeBSD/X86_64/ProcessX86_64.cpp",
        ],
        ":windows-armv7": [
            "Sources/Taret/Windows/ARM/ThreadARM.cpp",
        ],
        ":windows-i686": [
            "Sources/Target/Windows/X86/ThreadX86.cpp",
        ],
        ":windows-x86_64": [
            "Sources/Target/Windows/X86_64/ThreadX86_64.cpp",
        ],
    }),
)

cc_binary(
    name = "ds2",
    srcs = [
        ":headers",
        ":sources",
    ],
    copts = selects.with_or({
        ("@bazel_tools//tools/cpp:clang-cl", "@bazel_tools//tools/cpp:msvc"): [
            "/std:c++17"
        ],
        "//conditions:default": ["-std=c++17"],
    }),
    defines = [
        "DS2_GIT_HASH=\"\\\"\\\"\"",
    ] + select({
        "@platforms//os:android": [
            "HAVE_PROCESS_VM_READV",
            "HAVE_PROCESS_VM_WRITEV",
        ],
        "@platforms//os:linux": [
            "HAVE_ENUM_PTRACE_REQUEST",
            "HAVE_GETTID",
            "HAVE_POSIX_OPENPT",
            "HAVE_PROCESS_VM_READV",
            "HAVE_PROCESS_VM_WRITEV",
            "HAVE_SYS_PERSONALITY_H",
            "HAVE_TGKILL",
        ],
    }),
    includes = [
        "Headers",
    ],
    linkopts = select({
        "@platforms//os:macos": [],
        "@platforms//os:windows": [],
        "@platforms//os:freebsd": ["-pthread"],
        "//conditions:default": [
            "-pthread",
            "-ldl",
        ],
    }),
    deps = [
        "//Tools/JSObjects:jsobjects",
    ],
)
