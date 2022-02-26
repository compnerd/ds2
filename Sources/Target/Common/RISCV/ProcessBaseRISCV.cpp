// Copyright 2022 Saleem Abdulrasool <compnerd@compnerd.org>

#include "DebugServer2/Target/ProcessBase.h"

using ds2::Architecture::GDBDescriptor;
using ds2::Architecture::LLDBDescriptor;

namespace ds2 {
namespace Target {

GDBDescriptor const *ProcessBase::getGDBRegistersDescriptor() const {
#if __riscv_xlen == 32
  return &Architecture::RISCV32::GDB;
#elif __riscv_xlen == 64
  return &Architecture::RISCV64::GDB;
#elif __riscv_xlen == 128
  return &Architecture::RISCV128::GDB;
#endif
}

LLDBDescriptor const *ProcessBase::getLLDBRegistersDescriptor() const {
#if __riscv_xlen == 32
  return &Architecture::RISCV32::LLDB;
#elif __riscv_xlen == 64
  return &Architecture::RISCV64::LLDB;
#elif __riscv_xlen == 128
  return &Architecture::RISCV128::LLDB;
#endif
}

} // namespace Target
} // namespace ds2
