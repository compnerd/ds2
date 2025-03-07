# Debugging Android apps using lldb and ds2

ds2 can run as a debug server on an Android device or emulator to enable remote debugging of native
code in Android applications. While ds2 is capable of connecting to gdb, this guide will focus on
using [lldb](https://lldb.llvm.org/).

## Setup

### Installing adb
Debugging Android requires the [Android Debug Bridge (adb)](https://developer.android.com/tools/adb)
be installed on your workstation. It comes as part of the Android SDK Platform Tools, which can be
downloaded [here](https://developer.android.com/tools/releases/platform-tools#downloads) without
installing Android Studio.

### Enable Device Debugging
If you are using a physical Android device, debugging must be enabled via the device's
[developer options](https://developer.android.com/studio/debug/dev-options).

This step is unnecessary if you are using an Android emulator.

### Make the Application Debuggable
The Android application you intend to debug must have `andriod:debuggable="true"` in its
`AndroidManifest.xml` file:
```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    ...
    <application
        android:debuggable="true"
        ...

```
While the property can be set directly, it is typically set via a debug
[build variant](https://developer.android.com/build/build-variants).

### Grant the Application Network Permission
The application you intend to debug must have the `android.permission.INTERNET` permission declared
in its `AndroidManifest.xml` file:
```xml
    <uses-permission android:name="android.permission.INTERNET" />
```
If your application does not have this permission, you cannot debug it using ds2.

This requirement is necessary because:
1. ds2 must run in the context of an application's sandbox to debug the application
2. When running in an application's sandbox, ds2 is limited to the set of permissions granted to
that application
3. ds2 connects to the debugger via a TCP connection, so it requires network access

There are no other permissions required for ds2 to debug an Android application.

## Running ds2

### Deploying ds2 to the Android device
Use adb to deploy the ds2 binary from your workstation to the `/data/local/tmp` directory on your
Android device:

```bash
$ adb push /path/to/ds2 /data/local/tmp
```
> **_NOTE:_**  For the Android emulator, copy the `x86_64` version of ds2. For an Android device,
copy the `arm64-v8a` version.

Ensure the binary is executable using `chmod +x` in the device shell:
```bash
$ adb shell chmod +x /data/local/tmp/ds2
```
### Copying ds2 into your application's sandbox
ds2 can be executed directly from its `/data/local/tmp` location to debug programs launched via
`adb shell`. However, to debug processes in an Android application, ds2 must be run in the context
of that application's sandbox using `run-as`.

Android applications can read from the `/data/local/tmp` directory, but, per security policy, they
cannot execute progams from this location. To work-around this restriction, you must first copy the
ds2 binary to the application's private storage using `run-as cp`:
```bash
$ adb shell run-as com.example.TestApp cp /data/local/tmp/ds2 ./
```
This command copies the ds2 executable to root of the application's working directory (e.g.
`/data/user/0/com.example.TestApp`). Once copied, you can execute ds2 from this location to debug
the application.

To confirm ds2 can run in the sandbox, execute it with no arguments using `run-as`:
```
$ adb shell run-as com.example.TestApp ./ds2
Usage:
  ./ds2 [v]ersion
  ./ds2 [g]dbserver [options]
  ./ds2 [p]latform [options]
```
### Port forwarding
To connect the debugger from your workstation to an instance of ds2 running on your Android device,
use adb's port forwarding to forward a TCP port to use for the connection:
```bash
$ adb forward tpc:5432 tcp:5432
```
The exact port number you choose doesn't really matter as long as it is not already in use. Make
note of the port number since you will need it later.
### Running ds2
Launch ds2 on your Android device in "platform" mode. Tell it to listen on the same port number that
you forwarded with adb.
```bash
$ adb shell run-as com.example.TestApp ./ds2 platform --server --listen *:5432
```
ds2 will now block waiting for an incoming connection from a debugger. If this command fails, make
sure the application has network permission (see above) and that there isn't already an instance of
ds2 running with the same port number.

## Debugging with lldb

### Connecting
You are now ready to connect the debugger. Launch lldb from the command line:
```bash
$ lldb
```
From the `(lldb)` prompt, run `platform select remote-android`:
```bash
(lldb) platform select remote-android
  Platform: remote-android
 Connected: no
```
Connenct to the running ds2 instance using `platform connect connect://localhost:5432`, specifying
the same port number that ds2 is listening on in the connect URI:
```bash
(lldb) platform connect connect://localhost:5432
  Platform: remote-android
    Triple: aarch64-unknown-linux-android
OS Version: 34 (5.10.198-android13-4-00050-g12f3388846c3-ab11920634)
  Hostname: localhost
 Connected: yes
WorkingDir: /data/user/0/com.example.TestApp
    Kernel: #1 SMP PREEMPT Mon Jun 3 20:51:42 UTC 2024
```
Note the `WorkingDir` value: it should be the root of your application's data directory.
### Attaching to a process
With a platform connection established between lldb and ds2, you can now attach the debugger to a
running process using its process ID (pid). To determine the pid for the process you wish to debug,
list the processes running in your applications' sandbox with `platform process list`:
```bash
(lldb) platform process list
2 matching processes were found on "remote-android"

PID    PARENT USER       TRIPLE                         NAME
====== ====== ========== ============================== ============================
8298   8296   u0_a284    aarch64-unknown-linux-android  ds2
8883   1139   u0_a284    aarch64-unknown-linux-android  app_process64
```
Because ds2 is running in your application's sandbox, this command will list only processes that
are also running in the sandbox. You should see both the ds2 process and an application process for
each of your application's running processes. If you only see ds2, make sure your application is
is running and try again.

Once you know the pid for the process you wish to debug, use the `attach --pid` command to attach
the debugger to it:
```
(lldb) attach --pid 8883
Process 8883 stopped
* thread #1, name = 'example.TestApp', stop reason = signal SIGSTOP
    frame #0: 0x00000072cc1cad28 libc.so`__epoll_pwait + 8
libc.so`__epoll_pwait:
->  0x72cc1cad28 <+8>:  cmn    x0, #0x1, lsl #12 ; =0x1000
    0x72cc1cad2c <+12>: cneg   x0, x0, hi
    0x72cc1cad30 <+16>: b.hi   0xc6530        ; __set_errno_internal
    0x72cc1cad34 <+20>: ret
Executable module set to "/home/user/.lldb/module_cache/remote-android/.cache/00418409-0550-60A6-0094-DA0030D00989/app_process64".
Architecture set to: aarch64-unknown-linux-android0
(lldb)
```
This command spawns an additional ds2 instance (running in gdbserver mode) which attaches to the
process and sets-up the debug session with lldb.

Once attached, you can debug the process with standard gdb and lldb commands. An lldb tutorial can
be found at [llvm.org](https://lldb.llvm.org/use/tutorial.html).
