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

template <size_t Begin, size_t End>
uintptr_t bits(uintptr_t value) {
  uintptr_t mask = ((1 << ((End + 1) - Begin)) - 1) << Begin;
  return (value & mask) >> Begin;
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

    switch (bits<0, 6>(instructions)) {
    default:
      destination = location + 4;
      break;
    case 0x00000063: {                                      // BRANCH
      uint8_t rs1 = bits<15, 19>(instruction);
      uint8_t rs2 = bits<20, 24>(instruction);
      uintptr_t immediate = (bits< 8, 11>(instruction) <<  1)
                          | (bits<25, 30>(instruction) <<  5)
                          | (bits< 7,  7>(instruction) << 11)
                          | (bits<31, 31>(instruction) << 12);
      uintptr_t lhs = rs1 ? state.gp.regs[rs1] : 0;
      uintptr_t rhs = rs2 ? state.gp.regs[rs2] : 0;
      switch (bits<12, 14>(instruction)) {
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
      switch (bits<12, 14>(instruction)) {
      default:
        destination = location + 4;
        break;
      case 0: {                                             // JALR
        uint8_t rs = bits<15, 19>(instruction);
        uint8_t rd = bits< 7, 11>(instruction);
        uintptr_t immediate = bits<20, 31>(instruction);
        (void)rd;
        uintptr_t base = rs ? state.gp.regs[rs] : 0;
        destination = base + sext<11>(immediate);
        break;
      }
      }
      break;
    }
    case 0x0000006f: {                                      // JAL
      uintptr_t immediate = (bits<21, 30>(instruction) <<  1)
                          | (bits<20, 20>(instruction) << 11)
                          | (bits<12, 19>(instruction) << 12)
                          | (bits<31, 31>(instruction) << 20);
      destination = location + sext<20>(immediate);
      break;
    }
    }
  } else {                                                  // RVC
    destination = location + 2;
    switch (bits<0, 1>(instruction)) {
    default: break;
    case 0x01:
      switch (bits<13, 15>(instruction)) {
      default: break;
#if __riscv_len == 32
      case 1:                                               // C.JAL
        [[fallthrough]];
#endif
      case 5: {                                             // C.J
        uintptr_t immediate = (bits< 3,  5>(instruction) <<  1)
                            | (bits<11, 11>(instruction) <<  4)
                            | (bits< 2,  2>(instruction) <<  5)
                            | (bits< 7,  7>(instruction) <<  6)
                            | (bits< 6,  6>(instruction) <<  7)
                            | (bits< 9, 10>(instruction) <<  8)
                            | (bits< 8,  8>(instruction) << 10)
                            | (bits<12, 12>(instruction) << 11);
        destination = location + sext<11>(immediate);
        break;
      }
      case 6:                                               // C.BEQZ
        [[fallthrough]];
      case 7: {                                             // C.BNEZ
        uintptr_t immediate = (bits< 3,  4>(instruction) << 1)
                            | (bits<10, 11>(instruction) << 3)
                            | (bits< 2,  2>(instruction) << 5)
                            | (bits< 5,  6>(instruction) << 6)
                            | (bits<12, 12>(instruction) << 8);
        uint8_t rs = bits<7, 9>(instruction);
        bool eq = bits<13, 15>(instruction) == 6;
        destination = location +
                      ((eq ? state.gp.regs[rs + 8] == 0 : state.gp.regs[rs + 8])
                           ? sext<8>(immediate)
                           : 2);
        break;
      }
      }
    case 0x02:
      switch (bits<12, 15>(instruction)) {
      default: break;
      case 8:                                               // C.JR
        [[fallthrough]];
      case 9: {                                             // C.JALR
        if (bits<2, 6>(instruction))                        // rs2 MBZ
          break;

        uint8_t rs1 = bits<7, 11>(instruction);
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
