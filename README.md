# ds2

ds2 is a debug server designed to be used with [LLDB](http://lldb.llvm.org/) to
perform remote debugging of Linux, Android, FreeBSD, Windows targets.

### Platform Support

There is varying support for different platforms. While not all the possible
platforms are under CI coverage, DS2 has been known to run on the following
platforms at various times.

- [x] Anrdoid
  - [x] ARM64
  - [x] ARMv7
  - [x] X64
  - [x] X86

- [x] Darwin
  - [ ] ARM64
  - [x] X64
  - [x] X86

- [x] FreeBSD
  - [x] X64
  - [x] X86

- [x] Linux
  - [x] ARM64
  - [x] ARMv7
  - [x] RISCV64
  - [x] X64
  - [x] X86

- [x] MinGW
  - [ ] ARM64
  - [x] ARMv7
  - [x] X64
  - [x] X86

- [-] Tizen
  - [x] ARMv7
  - [x] X86

- [x] Windows
  - [ ] ARM64
  - [x] ARMv7
  - [x] X64
  - [x] X86

### Contributing

Patches are welcome to fix issues on existing platforms as well as to improve
coverage of existing or new platforms. For CI coverage, patches to increase
coverage under GitHub Actions or to migrate coverage from CircleCI are also
welcome.

### Documentation
- [Android](Documentation/Android.md)
- [Building](Documentation/Building.md)
- [Running](Documentation/Running.md)

### License

ds2 is licensed under the University of Illinois/NCSA Open Source License.

We also provide an additional patent grant which can be found in the `PATENTS`
file in the root directory of this source tree.

regsgen2, a tool used to generate register definitions is also licensed under
the University of Illinois/NCSA Open Source License and uses a json library
licensed under the Boost Software License. A complete copy of the latter can be
found in `Tools/libjson/LICENSE_1_0.txt`.

Note that neither regsgen2 nor libjson are part of the ds2 distribution itself,
but rather are used to generate code that is included in ds2.
