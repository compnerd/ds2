# ds2

ds2 is a debug server designed to be used with [LLDB](http://lldb.llvm.org/) to perform remote debugging of Linux, Android, FreeBSD, Windows targets.

## Building ds2

ds2 uses [CMake](http://www.cmake.org/) to generate its build system. A variety
of CMake toolchain files are provided to help with cross compilation for other
targets.

The build instructions include instructions assuming that the [ninja-build](https://github.com/ninja-build/ninja) is used for the build tool.  However, it is possible to use other build tools (e.g. make or msbuild).

### Requirements

**Generic**
  - CMake (https://cmake.org)
  - c++ (c++17 or newer)

**Windows Only**
  - Visual Studio 2015 or newer
  - WinFlexBison (https://github.com/lexxmark/winflexbison)

**non-Windows Platforms**
  - flex
  - bison

### Building with CMake + Ninja

> **Windows Only**

> [!NOTE]
> ds2 requires Visual Studio (with at least the Windows SDK, though the C/C++ workload support is recommended).  CMake can be installed as part of Visual Studio 2019 or through an official release if desired. The build currently requires flex and bison, which can be satisfied by the WinFlexBison project. Visual Studio, MSBuild, and Ninja generators are supported. The Ninja build tool can be installed through Visual Studio by installing the CMake support or can be downloaded from the project's home page.

After cloning the ds2 repository, run the following commands to build for the current host:

```sh
cmake -B out -G Ninja -S ds2
ninja -C out
```

### Compiling for Android

For Android native debugging, it is possible to build ds2 with the Android NDK.

```sh
cmake -B out -D CMAKE_SYSTEM_NAME=Android -D CMAKE_ANDROID_ARCH_ABI=armeabi-v7a -G Ninja -S ds2
ninja -C out
```

Note that this will build ds2 targeting the highest level API level that the
NDK supports. If you want to target another api level, e.g. 21, add the flag
`-DCMAKE_SYSTEM_VERSION=21` to your CMake invocation.

### Compiling for Linux ARM

Cross-compiling for Linux-ARM is also possible. On Ubuntu 14.04, install an arm
toolchain (for instance `g++-4.8-arm-linux-gnueabihf`) and use the provided
toolchain file.

```sh
cd ds2
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../Support/CMake/Toolchain-Linux-ARM.cmake ..
make
```

This will generate a binary that you can copy to your device to start
debugging.



## Running ds2

### Example

#### On the remote host

Launch ds2 with something like:

    $ ./ds2 gdbserver localhost:4242 /path/to/TestSimpleOutput

ds2 is now ready to accept connections on port 4242 from lldb.

#### On your local host

    $ lldb /path/to/TestSimpleOutput
    Current executable set to '/path/to/TestSimpleOutput' (x86_64).
    (lldb) gdb-remote localhost:4242
    Process 8336 stopped
    * thread #1: tid = 8336, 0x00007ffff7ddb2d0, name = 'TestSimpleOutput', stop reason = signal SIGTRAP
        frame #0: 0x00007ffff7ddb2d0
    -> 0x7ffff7ddb2d0:  movq   %rsp, %rdi
       0x7ffff7ddb2d3:  callq  0x7ffff7ddea70
       0x7ffff7ddb2d8:  movq   %rax, %r12
       0x7ffff7ddb2db:  movl   0x221b17(%rip), %eax
    (lldb) b main
    Breakpoint 1: where = TestSimpleOutput`main + 29 at TestSimpleOutput.cpp:6, address = 0x000000000040096d
    [... debug debug ...]
    (lldb) c
    Process 8336 resuming
    Process 8336 exited with status = 0 (0x00000000)
    (lldb)

### Command-Line Options

ds2 accepts the following options:

```
usage: ds2 [RUN_MODE] [OPTIONS] [[HOST]:PORT] [-- PROGRAM [ARGUMENTS...]]
  -a, --attach ARG           attach to the name or PID specified
  -f, --daemonize            detach and become a daemon
  -d, --debug                enable debug log output
  -F, --fd ARG               use a file descriptor to communicate
  -g, --gdb-compat           force ds2 to run in gdb compat mode
  -o, --log-file ARG         output log messages to the file specified
  -N, --named-pipe ARG       determine a port dynamically and write back to FIFO
  -n, --no-colors            disable colored output
  -D, --remote-debug         enable log for remote protocol packets
  -R, --reverse-connect      connect back to the debugger at [HOST]:PORT
  -e, --set-env ARG...       add an element to the environment before launch
  -S, --setsid               make ds2 run in its own session
  -E, --unset-env ARG...     remove an element from the environment before lauch
  -l, --listen ARG           specify the [host]:port to listen on
  [host]:port                the [host]:port to connect to
```

After building ds2 for your target, run it with the binary to debug, or attach
to an already running process. Then, start LLDB as usual and attach to the ds2
instance with the `gdb-remote` command.

The run mode and port number must be specified, where run mode is either
`g[dbserver]` or `p[latform]`. In most cases, the `g[dbserver]` option is desired.

## License

ds2 is licensed under the University of Illinois/NCSA Open Source License.

We also provide an additional patent grant which can be found in the `PATENTS`
file in the root directory of this source tree.

regsgen2, a tool used to generate register definitions is also licensed under
the University of Illinois/NCSA Open Source License and uses a json library
licensed under the Boost Software License. A complete copy of the latter can be
found in `Tools/libjson/LICENSE_1_0.txt`.
