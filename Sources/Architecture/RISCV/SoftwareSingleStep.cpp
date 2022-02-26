// Copyright 2022 Saleem Abdulrasool <compnerd@compnerd.org>

#include "DebugServer2/Architecture/RISCV/SoftwareSingleStep.h"
#include "DebugServer2/Utils/Log.h"

namespace ds2 {
namespace Architecture {
namespace RISCV {

namespace {
template <size_t Width>
uintptr_t sext(uintptr_t value) {
  return ((value & (1 << Width)) ? ~((value & (1 << Width)) - 1) : 0) |
         (value & ((1 << Width + 1) - 1));
}
}

ErrorCode PrepareSoftwareSingleStep(Target::Process *process,
                                    BreakpointManager *manager,
                                    CPUState const &state,
                                    Address const &address) {
  uintptr_t location = address.valid() ? address.value() : state.pc();

  uint16_t instruction;
  CHK(process->readMemory(location, &instruction, sizeof(instruction)));

  uintptr_t destination;
  if ((instruction & 0x3) == 0x3) {                         // RVI
    uint32_t instruction;
    CHK(process->readMemory(location, &instruction, sizeof(instruction)));

    switch (instruction & 0x0000007f) {
    default:
      destination = location + 4;
      break;
    case 0x00000063: {                                      // BRANCH
      uint8_t rs1 = ((instruction & 0x000f8000) >> 15);
      uint8_t rs2 = ((instruction & 0x01f00000) >> 20);
      uintptr_t immediate = (((instruction & 0x00000f00) >>  8) <<  1)
                          | (((instruction & 0x7e000000) >> 25) <<  5)
                          | (((instruction & 0x00000080) >>  7) << 11)
                          | (((instruction & 0x80000000) >> 31) << 12);
      uintptr_t lhs = rs1 ? state.gp.regs[rs1] : 0;
      uintptr_t rhs = rs2 ? state.gp.regs[rs2] : 0;
      switch ((instruction & 0x00007000) >> 12) {
      case 0: // EQ
        destination = location + (lhs == rhs ? sext<12>(immediate) : 4);
        break;
      case 1: // NE
        destination = location + (lhs == rhs ? 4 : sext<12>(immediate));
        break;
      case 2:
        DS2LOG(Debug, "unknown branch condition funct3=2");
        DS2BUG("unknown branch condition 2");
      case 3:
        DS2LOG(Debug, "unknown branch condition funct3=3");
        DS2BUG("unknown branch condition 3");
      case 4: // LT
        destination =
            location + (static_cast<intptr_t>(lhs) < static_cast<intptr_t>(rhs)
                            ? sext<12>(immediate)
                            : 4);
        break;
      case 5: // GE
        destination =
            location + (static_cast<intptr_t>(lhs) >= static_cast<intptr_t>(rhs)
                            ? sext<12>(immediate)
                            : 4);
        break;
      case 6: // LTU
        destination = location + (lhs < rhs ? sext<12>(immediate) : 4);
        break;
      case 7: // GTU
        destination = location + (lhs >= rhs ? sext<12>(immediate) : 4);
        break;
      }
      break;
    }
    case 0x00000067: {
      switch (instruction & 0x00007000) {
      default:
        destination = location + 4;
        break;
      case 0: {                                             // JALR
        uint8_t rs = ((instruction & 0x000f8000) >> 15);
        uint8_t rd = ((instruction & 0x00000f80) >>  7);
        uintptr_t immediate = (instruction & 0xfff00000) >> 20;
        (void)rd;
        uintptr_t base = rs ? state.gp.regs[rs] : 0;
        destination = base + sext<11>(immediate);
        break;
      }
      }
      break;
    }
    case 0x0000006f: {                                      // JAL
      uintptr_t immediate = (((instruction & 0x7fe00000) >> 21) <<  1)
                          | (((instruction & 0x00100000) >> 20) << 11)
                          | (((instruction & 0x000ff000) >> 12) << 12)
                          | (((instruction & 0x80000000) >> 31) << 20);
      destination = location + sext<20>(immediate);
      break;
    }
    }
  } else {                                                  // RVC
    destination = location + 2;
    switch ((instruction & 0x3) >>  0) {
    default: break;
    case 0x01:
      switch ((instruction & 0xe000) >> 13) {
      default: break;
#if __riscv_len == 32
      case 1:                                               // C.JAL
        [[fallthrough]];
#endif
      case 5: {                                             // C.J
        uintptr_t immediate = (((instruction & 0x0038) >>  3) <<  1)
                            | (((instruction & 0x0800) >> 11) <<  4)
                            | (((instruction & 0x0004) >>  2) <<  5)
                            | (((instruction & 0x0080) >>  7) <<  6)
                            | (((instruction & 0x0040) >>  6) <<  7)
                            | (((instruction & 0x0600) >>  9) <<  8)
                            | (((instruction & 0x0100) >>  8) << 10)
                            | (((instruction & 0x1000) >> 12) << 11);
        destination = location + sext<11>(immediate);
        break;
      }
      case 6:                                               // C.BEQZ
        [[fallthrough]];
      case 7: {                                             // C.BNEZ
        uintptr_t immediate = (((instruction & 0x0018) >>  3) << 1)
                            | (((instruction & 0x0c00) >> 10) << 3)
                            | (((instruction & 0x0004) >>  2) << 5)
                            | (((instruction & 0x0060) >>  5) << 6)
                            | (((instruction & 0x1000) >> 12) << 8);
        uint8_t rs = (instruction & 0x0380) >> 7;
        bool eq = ((instruction & 0xe000) >> 13) == 6;
        destination = location +
                      ((eq ? state.gp.regs[rs + 8] == 0 : state.gp.regs[rs + 8])
                           ? sext<8>(immediate)
                           : 2);
        break;
      }
      }
    case 0x02:
      switch ((instruction & 0xf000) >> 12) {
      default: break;
      case 8:                                               // C.JR
        [[fallthrough]];
      case 9: {                                             // C.JALR
        if ((instruction & 0x007c) >> 2)
          break;

        uint8_t rs1 = ((instruction & 0x0f80) >> 7);
        destination = state.gp.regs[rs1];
        break;
      }
      }
    }
  }

  CHK(process->readMemory(destination, &instruction, sizeof(instruction)));
  CHK(manager->add(destination, BreakpointManager::Lifetime::TemporaryOneShot,
                   (instruction & 0x0003) == 0x0003 ? 4 : 2,
                   BreakpointManager::kModeExec));
  return kSuccess;
}

} // namespace RISCV
} // namespace Architecture
} // namespace ds2
