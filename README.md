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

**Android**
  - Android NDK r18b or newer (https://developer.android.com/ndk/downloads)

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
An NDK version of at least r18b is required for C++17 support.

CMake will automatically find the installed Android NDK if either the
`ANDROID_NDK_ROOT` or `ANDROID_NDK` environment variable is set. Alternatively,
the NDK location can be provided to CMake directly with the `CMAKE_ANDROID_NDK`
variable, e.g. `-DCMAKE_ANDROID_NDK=/home/user/Android/Sdk/ndk/26.1.10909125`.

To build for Android:
```sh
cd ds2
cmake -B out -S . -D CMAKE_SYSTEM_NAME=Android -D CMAKE_ANDROID_ARCH_ABI=arm64-v8a -D CMAKE_BUILD_TYPE=Release -G Ninja
cmake --build out
```

By default, CMake will build ds2 targeting the highest level API level that the
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

### Running on a remote host

Launch ds2 in platform mode (not supported on Windows):
```sh
$ ./ds2 platform --server --listen localhost:4242
```

Launch ds2 as a single-instance gdb server debugging a program:
```sh
$ ./ds2 gdbserver localhost:4242 /path/to/TestSimpleOutput
```

In both cases, ds2 is ready to accept connections on port 4242 from lldb.

### Running on an Android device

When debugging Android NDK programs or applications, an Android device or
emulator must be must be connected to the host machine and accessible via `adb`:
```sh
$ adb devices
List of devices attached
emulator-5554   device
```

To use ds2 on the Android target, copy the locally build ds2 binary to the
`/data/local/tmp` directory on the Android target using `adb push` and launch it
from there:
```sh
$ adb push build/ds2 /data/local/tmp
$ adb shell /data/local/tmp/ds2 platform --server --listen *:4242
```

> [!NOTE]
> If built on a Windows host, the ds2 executable file must also be marked
> executable before launching it:
> ```sh
> $ adb shell chmod +x /data/local/tmp/ds2
> ```

The port that ds2 is listening on must be forwarded to the connected host
using `adb forward`:
```sh
$ adb forward tcp:4242 tcp:4242
```

When debugging an Android application, the ds2 binary must be copied from
`/data/local/tmp` into the target application's sandbox directory. This can be
done using Android's `run-as` command:
```sh
$ adb shell run-as com.example.app cp /data/local/tmp/ds2 ./ds2
$ adb shell run-as com.example.app ./ds2 platform --server --listen *:4242
```

> [!NOTE]
> When running in an Android application's sandbox, the target application must
> have internet permissions or ds2 will fail to open a port on launch:
> ```xml
> <uses-permission android:name="android.permission.INTERNET" />
> <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
> ```

### Run lldb client on the local host

#### Platform Mode
If ds2 was launched in `platform` mode (not supported on Windows), lldb can
connect to it using `platform` commands.

For a remote Linux host:
```
$ lldb
(lldb) platform select remote-linux
(lldb) platform connect connect://localhost:4242
```

For a remote Android host:
```
$ lldb
(lldb) platform select remote-android
(lldb) platform connect connect://localhost:4242
(lldb) platform settings -w /data/local/tmp
```

> [!NOTE]
> When running in an Android application's sandbox, the `platform settings -w`
> command, which sets the working directory, is not necessary because the
> it is already set to the root of the application's writable sandbox directory.

Once connected in platform mode, you can select the program to be run using the
`file` command, run, and debug.
```
(lldb) file /path/to/executable
(lldb) b main
(lldb) run
```

#### Gdb Server Mode
If ds2 was launched in `gdbserver` mode, lldb can connect to it with the
`gdb-remote` command:
```
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
```

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
