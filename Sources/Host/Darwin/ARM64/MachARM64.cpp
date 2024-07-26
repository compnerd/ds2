//
// Copyright (c) 2024-present, Saleem Abdulrasool <compnerd@compnerd.org>
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Host/Darwin/Mach.h"
#include "DebugServer2/Host/Platform.h"
#include "DebugServer2/Utils/Log.h"

namespace ds2 {
namespace Host {
namespace Darwin {

ErrorCode Mach::readCPUState(const ProcessThreadId &tid, const ProcessInfo &info,
                             Architecture::CPUState &state) {
  if (!tid.valid())
    return kErrorInvalidArgument;

  thread_t thread = getMachThread(tid);
  if (thread == THREAD_NULL)
    return kErrorProcessNotFound;

  mach_msg_type_number_t state_count = ARM_THREAD_STATE64_COUNT;
  arm_thread_state64_t thread_state;
  kern_return_t result;

  result = thread_get_state(thread, ARM_THREAD_STATE64,
                            reinterpret_cast<thread_state_t>(&thread_state),
                            &state_count);
  if (result != KERN_SUCCESS)
    return kErrorInvalidArgument;

  state.isA32 = false;
  state.state64.gp.x0 = thread_state.__x[0];
  state.state64.gp.x1 = thread_state.__x[1];
  state.state64.gp.x2 = thread_state.__x[2];
  state.state64.gp.x3 = thread_state.__x[3];
  state.state64.gp.x4 = thread_state.__x[4];
  state.state64.gp.x5 = thread_state.__x[5];
  state.state64.gp.x6 = thread_state.__x[6];
  state.state64.gp.x7 = thread_state.__x[7];
  state.state64.gp.x8 = thread_state.__x[8];
  state.state64.gp.x9 = thread_state.__x[9];
  state.state64.gp.x10 = thread_state.__x[10];
  state.state64.gp.x11 = thread_state.__x[11];
  state.state64.gp.x12 = thread_state.__x[12];
  state.state64.gp.x13 = thread_state.__x[13];
  state.state64.gp.x14 = thread_state.__x[14];
  state.state64.gp.x15 = thread_state.__x[15];
  state.state64.gp.x16 = thread_state.__x[16];
  state.state64.gp.x17 = thread_state.__x[17];
  state.state64.gp.x18 = thread_state.__x[18];
  state.state64.gp.x19 = thread_state.__x[19];
  state.state64.gp.x20 = thread_state.__x[20];
  state.state64.gp.x21 = thread_state.__x[21];
  state.state64.gp.x22 = thread_state.__x[22];
  state.state64.gp.x23 = thread_state.__x[23];
  state.state64.gp.x24 = thread_state.__x[24];
  state.state64.gp.x25 = thread_state.__x[25];
  state.state64.gp.x26 = thread_state.__x[26];
  state.state64.gp.x27 = thread_state.__x[27];
  state.state64.gp.x28 = thread_state.__x[28];
  state.state64.gp.fp = arm_thread_state64_get_fp(thread_state);
  state.state64.gp.lr = arm_thread_state64_get_lr(thread_state);
  state.state64.gp.sp = arm_thread_state64_get_sp(thread_state);
  state.state64.gp.pc = arm_thread_state64_get_pc(thread_state);
  state.state64.gp.cpsr = thread_state.__cpsr;

  return kSuccess;
}

ErrorCode Mach::writeCPUState(const ProcessThreadId &tid, const ProcessInfo &info,
                              const Architecture::CPUState &state) {
  if (!tid.valid())
    return kErrorInvalidArgument;

  thread_t thread = getMachThread(tid);
  if (thread == THREAD_NULL)
    return kErrorProcessNotFound;

  arm_thread_state64_t thread_state;
  thread_state.__x[0] = state.state64.gp.x0;
  thread_state.__x[1] = state.state64.gp.x1;
  thread_state.__x[2] = state.state64.gp.x2;
  thread_state.__x[3] = state.state64.gp.x3;
  thread_state.__x[4] = state.state64.gp.x4;
  thread_state.__x[5] = state.state64.gp.x5;
  thread_state.__x[6] = state.state64.gp.x6;
  thread_state.__x[7] = state.state64.gp.x7;
  thread_state.__x[8] = state.state64.gp.x8;
  thread_state.__x[9] = state.state64.gp.x9;
  thread_state.__x[10] = state.state64.gp.x10;
  thread_state.__x[11] = state.state64.gp.x11;
  thread_state.__x[12] = state.state64.gp.x12;
  thread_state.__x[13] = state.state64.gp.x13;
  thread_state.__x[14] = state.state64.gp.x14;
  thread_state.__x[15] = state.state64.gp.x15;
  thread_state.__x[16] = state.state64.gp.x16;
  thread_state.__x[17] = state.state64.gp.x17;
  thread_state.__x[18] = state.state64.gp.x18;
  thread_state.__x[19] = state.state64.gp.x19;
  thread_state.__x[20] = state.state64.gp.x20;
  thread_state.__x[21] = state.state64.gp.x21;
  thread_state.__x[22] = state.state64.gp.x22;
  thread_state.__x[23] = state.state64.gp.x23;
  thread_state.__x[24] = state.state64.gp.x24;
  thread_state.__x[25] = state.state64.gp.x25;
  thread_state.__x[26] = state.state64.gp.x26;
  thread_state.__x[27] = state.state64.gp.x27;
  thread_state.__x[28] = state.state64.gp.x28;
  arm_thread_state64_set_fp(thread_state, state.state64.gp.fp);
  arm_thread_state64_set_lr_fptr(thread_state, state.state64.gp.lr);
  arm_thread_state64_set_sp(thread_state, state.state64.gp.sp);
  arm_thread_state64_set_pc_fptr(thread_state, state.state64.gp.pc);
  thread_state.__cpsr = state.state64.gp.cpsr;

  kern_return_t result;
  result = thread_set_state(thread, ARM_THREAD_STATE64,
                            reinterpret_cast<thread_state_t>(&thread_state),
                            ARM_THREAD_STATE64_COUNT);
  return result == KERN_SUCCESS ? kSuccess : kErrorInvalidArgument;
}

}
}
}
