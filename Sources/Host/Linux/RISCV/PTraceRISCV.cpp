// Copyright 2022 Saleem Abdulrasool <compnerd@compnerd.org>

#include "DebugServer2/Host/Linux/PTrace.h"
#include "DebugServer2/Core/HardwareBreakpointManager.h"
#include "DebugServer2/Host/Linux/ExtraWrappers.h"
#include "DebugServer2/Host/Platform.h"

#include <asm/ptrace.h>
#include <elf.h>

#include <cstring>

#define super ds2::Host::POSIX::PTrace

namespace ds2 {
namespace Host {
namespace Linux {

ErrorCode PTrace::readCPUState(ProcessThreadId const &ptid, ProcessInfo const &,
                               Architecture::CPUState &state) {
  struct user_regs_struct gprs;
  CHK(readRegisterSet(ptid, NT_PRSTATUS, &gprs, sizeof(gprs)));

  // The layout is identical.
  std::memcpy(state.gp.regs, &gprs, sizeof(state.gp.regs));

#if __riscv_flen == 32
  struct __riscv_f_ext_state fprs;
  CHK(readRegisterSet(ptid, NT_FPREGSET, &fprs, sizeof(fprs)));
#elif __riscv_flen == 64
  struct __riscv_d_ext_state fprs;
  CHK(readRegisterSet(ptid, NT_FPREGSET, &fprs, sizeof(fprs)));
#elif __riscv_flen == 128
#error "quad precision floating point support unimplemented"
#endif

  // The layout is identical.
  std::memcpy(&state.fp, &fprs, sizeof(state.fp));

  return kSuccess;
}

ErrorCode PTrace::writeCPUState(ProcessThreadId const &ptid,
                                ProcessInfo const &,
                                Architecture::CPUState const &state) {
  struct user_regs_struct gprs;
  static_assert(sizeof(gprs) >= sizeof(state.gp.regs),
                "expected `struct user_regs_struct` to be larger");
  // The layout is identical.
  std::memcpy(&gprs, state.gp.regs, sizeof(state.gp.regs));

  CHK(writeRegisterSet(ptid, NT_PRSTATUS, &gprs, sizeof(gprs)));

#if __riscv_flen == 32
  struct __riscv_f_ext_state fprs;
  static_assert(sizeof(fprs) >= sizeof(state.fp),
                "expected `struct __riscv_f_ext_state` to be larger");
#elif __riscv_flen == 64
  struct __riscv_d_ext_state fprs;
  static_assert(sizeof(fprs) >= sizeof(state.fp),
                "expected `struct __riscv_d_ext_state` to be larger");
#elif __riscv_flen == 128
#error "quad precision floating point support unimplemented"
#endif

  // The layout is identical.
  std::memcpy(&fprs, &state.fp, sizeof(state.fp));

  CHK(writeRegisterSet(ptid, NT_FPREGSET, &fprs, sizeof(fprs)));

  return kSuccess;
}

} // namespace Linux
} // namespace Host
} // namespace ds2
