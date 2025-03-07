## Running ds2

### Running on a remote host

Launch ds2 in platform mode (not supported on Windows):
```sh
$ ./ds2 platform --server --listen localhost:4242
```

Launch ds2 as a single-instance gdb server debugging a program:
```sh
$ ./ds2 gdbserver localhost:4242 /path/to/executable
```

In both cases, ds2 is ready to accept connections on port 4242 from LLDB.

### Running on an Android device

When debugging Android NDK programs or applications, an Android device or
emulator must be must be connected to the host machine and accessible via `adb`:
```sh
$ adb devices
List of devices attached
emulator-5554   device
```

The port that ds2 is listening on must be forwarded to the connected host
using `adb forward`:
```sh
$ adb forward tcp:4242 tcp:4242
```

To use ds2 on the Android target, copy the locally built ds2 binary to the
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

### Run LLDB client on the local host

#### Platform Mode
If ds2 was launched in `platform` mode (not supported on Windows), LLDB can
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

#### GDB Server Mode
If ds2 was launched in `gdbserver` mode, LLDB can connect to it with the
`gdb-remote` command:
```
$ lldb /path/to/executable
Current executable set to '/path/to/executable' (x86_64).
(lldb) gdb-remote localhost:4242
Process 8336 stopped
* thread #1: tid = 8336, 0x00007ffff7ddb2d0, name = 'executable', stop reason = signal SIGTRAP
    frame #0: 0x00007ffff7ddb2d0
-> 0x7ffff7ddb2d0:  movq   %rsp, %rdi
   0x7ffff7ddb2d3:  callq  0x7ffff7ddea70
   0x7ffff7ddb2d8:  movq   %rax, %r12
   0x7ffff7ddb2db:  movl   0x221b17(%rip), %eax
(lldb) b main
Breakpoint 1: where = executable`main + 29 at test.cpp:6, address = 0x000000000040096d
[...]
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

After building ds2 for your host, run it with the binary to debug, or attach
to an already running process. Then, start LLDB as usual and attach to the ds2
instance with the `gdb-remote` command.

The run mode and port number must be specified, where run mode is either
`g[dbserver]` or `p[latform]`. In most cases, the `g[dbserver]` option is desired.
