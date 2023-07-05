// Copyright 2022 Saleem Abdulrasool <compnerd@compnerd.org>

#include "DebugServer2/Core/SoftwareBreakpointManager.h"
#include "DebugServer2/Target/Process.h"
#include "DebugServer2/Target/Thread.h"

#define super ds2::BreakpointManager

namespace ds2 {

int SoftwareBreakpointManager::hit(Target::Thread *thread, Site &site) {
  ds2::Architecture::CPUState state;
  thread->readCPUState(state);
  return super::hit(state.pc(), site) ? 0 : -1;
}

void SoftwareBreakpointManager::getOpcode(size_t size,
                                          ByteVector &opcode) const {
  opcode.clear();

  switch (size) {
  case 2: // c.ebreak
    opcode.push_back('\x02');
    opcode.push_back('\x90');
    break;
  case 4: // ebreak
    opcode.push_back('\x73');
    opcode.push_back('\x00');
    opcode.push_back('\x10');
    opcode.push_back('\x00');
    break;
  default:
    DS2LOG(Error, "unsupported breakpoint width '%zu'", size);
    DS2BUG("unsupported breakpoint width");
    break;
  }
}

ErrorCode SoftwareBreakpointManager::isValid(Address const &address,
                                             size_t size, Mode mode) const {
  DS2ASSERT(mode == kModeExec);
  if (size == 0 || size == 2 || size == 4)
    return super::isValid(address, size, mode);
  DS2LOG(Debug, "received unsupported breakpoint size '%zu'", size);
  return kErrorInvalidArgument;
}

size_t
SoftwareBreakpointManager::chooseBreakpointSize(Address const &address) const {
  uint16_t instruction;
  CHK(_process->readMemory(address.value(), &instruction, sizeof(instruction)));

  // TODO(compnerd) move this into ds2::Architecture::RISCV::GetInstructionSize
  if ((instruction & 0x03) != 0x03) // RVC
    return 2;
  if ((instruction & 0x1f) != 0x1f) // RVI
    return 4;
  // TODO(compnerd) handle 48-bit, 64-bit, and larger instructions
  return 2;
}

} // namespace ds2
