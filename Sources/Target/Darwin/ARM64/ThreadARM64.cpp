//
// Copyright (c) 2024, Saleem Abdulrasool <compnerd@compnerd.org>
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Target/Darwin/Thread.h"
#include "DebugServer2/Architecture/CPUState.h"
#include "DebugServer2/Target/Process.h"

namespace ds2 {
namespace Target {
namespace Darwin {

namespace {
// PSTATE.SS, per the ARMv8 architecture reference manual.
constexpr uint64_t kPSTATE_SS = 1ULL << 21;
}

ErrorCode Thread::step(int signal, Address const &address) {
  ErrorCode error = modifyRegisters([](Architecture::CPUState &state) {
    state.state64.gp.cpsr |= kPSTATE_SS;
  });
  if (error != kSuccess)
    return error;

  error = process()->mach().setSingleStep(
      ProcessThreadId(process()->pid(), tid()), true);
  if (error != kSuccess)
    return error;

  error = resume(signal, address);
  if (error != kSuccess)
    return error;

  // resume() sets _state to kRunning; Process::wait()'s ignored-signal path
  // checks kStepped to decide whether to re-issue a step or a plain resume
  // if this step is interrupted before it completes, so this has to be set
  // after resume(), not folded into it.
  _state = kStepped;
  return kSuccess;
}

ErrorCode Thread::afterResume() {
  ErrorCode error = modifyRegisters([](Architecture::CPUState &state) {
    state.state64.gp.cpsr &= ~kPSTATE_SS;
  });
  if (error != kSuccess)
    return error;

  return process()->mach().setSingleStep(
      ProcessThreadId(process()->pid(), tid()), false);
}
}
}
}
