//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Target/Thread.h"
#include "DebugServer2/Host/Platform.h"
#include "DebugServer2/Host/Windows/ExtraWrappers.h"
#include "DebugServer2/Target/Process.h"
#include "DebugServer2/Utils/Log.h"

#include <cstring>
#include <windows.h>

using ds2::Host::Platform;

namespace ds2 {
namespace Target {
namespace Windows {

ErrorCode Thread::step(int signal, Address const &address) {
  // TODO(compnerd) AArch64 single-step requires setting the SS bit in
  // MDSCR_EL1, which is not accessible from user mode via
  // Get/SetThreadContext. Implement software single-step (as is done for
  // 32-bit ARM) once an AArch64 instruction decoder is available.
  return kErrorUnsupported;
}

ErrorCode Thread::readCPUState(Architecture::CPUState &state) {
  // TODO(compnerd) Handle floating point/NEON and debug registers.
  DWORD flags = CONTEXT_INTEGER | // X0-X28.
                CONTEXT_CONTROL;  // Fp, Lr, Sp, Pc, Cpsr.

  ProcessInfo pinfo;
  CHK(process()->getInfo(pinfo));

  DS2ASSERT(pinfo.pointerSize == sizeof(uint64_t));

  CONTEXT context;

  std::memset(&context, 0, sizeof(context));
  context.ContextFlags = flags;

  BOOL result = GetThreadContext(_handle, &context);
  if (!result) {
    return Platform::TranslateError();
  }

  state.isA32 = false;

  // X0-X28, Fp, Lr share an identical layout between the Windows CONTEXT and
  // our GP register block.
  std::memcpy(state.state64.gp.regs, context.X, sizeof(context.X));
  state.state64.gp.sp = context.Sp;
  state.state64.gp.pc = context.Pc;
  state.state64.gp.cpsr = context.Cpsr;

  return kSuccess;
}

ErrorCode Thread::writeCPUState(Architecture::CPUState const &state) {
  // TODO(compnerd) Handle floating point/NEON and debug registers.
  DWORD flags = CONTEXT_INTEGER   // X0-X28.
              | CONTEXT_CONTROL;  // Fp, Lr, Sp, Pc, Cpsr.

  ProcessInfo pinfo;
  CHK(process()->getInfo(pinfo));

  DS2ASSERT(pinfo.pointerSize == sizeof(uint64_t));

  CONTEXT context;

  std::memset(&context, 0, sizeof(context));
  context.ContextFlags = flags;

  std::memcpy(context.X, state.state64.gp.regs, sizeof(context.X));
  context.Sp = state.state64.gp.sp;
  context.Pc = state.state64.gp.pc;
  context.Cpsr = static_cast<DWORD>(state.state64.gp.cpsr);

  BOOL result = SetThreadContext(_handle, &context);
  if (!result) {
    return Platform::TranslateError();
  }

  return kSuccess;
}
} // namespace Windows
} // namespace Target
} // namespace ds2
