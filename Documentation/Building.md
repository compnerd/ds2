## Building ds2

ds2 uses [CMake](http://www.cmake.org/) to generate its build system. A variety
of CMake toolchain files are provided to help with cross compilation for other
targets.

The build instructions include instructions assuming that the
[ninja-build](https://github.com/ninja-build/ninja) is used for the build tool.
However, it is possible to use other build tools (e.g. make or msbuild).

### Requirements

**Generic**
  - CMake (https://cmake.org)
  - c++ (c++17 or newer)

**Windows Only**
  - Visual Studio 2015 or newer
  - WinFlexBison (https://github.com/lexxmark/winflexbison)

  > [!NOTE]
  > ds2 requires Visual Studio (with at least the Windows SDK, though the C/C++ workload support is recommended).  CMake can be installed as part of Visual Studio 2019 or through an official release if desired. The build currently requires flex and bison, which can be satisfied by the WinFlexBison project. Visual Studio, MSBuild, and Ninja generators are supported. The Ninja build tool can be installed through Visual Studio by installing the CMake support or can be downloaded from the project's home page.

**non-Windows Platforms**
  - flex
  - bison

**Android**
  - Android NDK r18b or newer (https://developer.android.com/ndk/downloads)

### Building with CMake + Ninja

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
variable, e.g. `-D CMAKE_ANDROID_NDK=/home/user/Android/Sdk/ndk/26.1.10909125`.

To build for Android:
```sh
cmake -B out -G Ninja -S ds2 -D CMAKE_SYSTEM_NAME=Android -D CMAKE_ANDROID_ARCH_ABI=arm64-v8a -D CMAKE_BUILD_TYPE=Release
ninja -C out
```

By default, CMake will build ds2 targeting the highest level API level that the
NDK supports. If you want to target another API level, e.g. 21, add the flag
`-D CMAKE_SYSTEM_VERSION=21` to your CMake invocation.

### Compiling for Linux ARM

Cross-compiling for Linux-ARM is also possible. On Ubuntu 14.04, install an arm
toolchain (for instance `g++-4.8-arm-linux-gnueabihf`) and use the provided
toolchain file.

```sh
cmake -B out -G Ninja -S ds2 -D CMAKE_TOOLCHAIN_FILE=ds2/Support/CMake/Toolchain-Linux-ARM.cmake
ninja -C out
```

This will generate a binary that you can copy to your device to start
debugging.
