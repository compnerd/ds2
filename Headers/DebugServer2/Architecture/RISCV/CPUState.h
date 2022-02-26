// Copyright 2022 Saleem Abdulrasool <compnerd@compnerd.org>

#pragma once

#if !defined(CPUSTATE_H_INTERNAL)
#error "You shall not include this file directly."
#endif

#if __riscv_xlen == 32
#include "DebugServer2/Architecture/RISCV32/RegistersDescriptors.h"
#elif __riscv_xlen == 64
#include "DebugServer2/Architecture/RISCV64/RegistersDescriptors.h"
#elif __riscv_xlen == 128
#include "DebugServer2/Architecture/RISCV128/RegistersDescriptors.h"
#endif

#include <algorithm>

namespace ds2 {
namespace Architecture {
namespace RISCV {

#pragma pack(push)

struct CPUState {
  union {
    uintptr_t regs[32];
    struct {
      // NOTE: pc takes the x0 slot to match `struct user_reg_state`.  x0 is
      // hardwired to zero, so the value can always be materialized.
      uintptr_t pc, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14,
          x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28,
          x29, x30, x31;
    };
  } gp = {0};

  // TODO(compnerd) support quad precision
  struct {
    union {
      uint32_t sng[32];
      uint64_t dbl[32];
    };
    uint32_t fcsr;
  } fp = {0};

public:
  //
  // Accessors
  //

  inline uintptr_t pc() const { return gp.pc; }
  inline void setPC(uintptr_t pc) { gp.pc = pc; }

  inline uintptr_t retval() const { return gp.x1; }

public:
  inline void getGPState(GPRegisterValueVector &regs) const {
    regs.clear();
    regs.emplace_back(GPRegisterValue{sizeof(uintptr_t), 0x0});
    std::transform(std::next(std::begin(gp.regs)), std::end(gp.regs),
                   std::back_inserter(regs),
                   [](uintptr_t reg) -> GPRegisterValue {
                     return GPRegisterValue{sizeof(reg), reg};
                   });
    regs.emplace_back(GPRegisterValue{sizeof(uintptr_t), gp.pc});
  }

  inline void setGPState(std::vector<uintptr_t> const &regs) {
    for (size_t reg = 1, nregs = std::min(std::size(regs), std::size(gp.regs));
         reg < nregs; ++reg)
      gp.regs[reg % (std::size(gp.regs) - 1)] = regs[reg];
  }

public:
  inline void getStopGPState(GPRegisterStopMap &regs, bool forLLDB) const {
#if __riscv_xlen == 32
    using namespace ds2::Architecture::RISCV32;
#elif __riscv_xlen == 64
    using namespace ds2::Architecture::RISCV64;
#elif __riscv_xlen == 128
    using namespace ds2::Architecture::RISCV128;
#endif

    if (forLLDB) {
      regs[reg_lldb_x0] = GPRegisterValue{sizeof(uintptr_t), 0};
      for (size_t reg = 1, nregs = std::size(gp.regs); reg < nregs; ++reg)
        regs[reg_lldb_x0 + reg] =
            GPRegisterValue{sizeof(uintptr_t), gp.regs[reg]};
        regs[reg_lldb_pc] = GPRegisterValue{sizeof(uintptr_t), gp.pc};
    } else {
      regs[reg_gdb_x0] = GPRegisterValue{sizeof(uintptr_t), 0};
      for (size_t reg = 1, nregs = std::size(gp.regs); reg < nregs; ++reg)
        regs[reg_gdb_x0 + reg] =
            GPRegisterValue{sizeof(uintptr_t), gp.regs[reg]};
        regs[reg_gdb_pc] = GPRegisterValue{sizeof(uintptr_t), gp.pc};
    }
  }

public:
  inline bool getLLDBRegisterPtr(int regno, void **ptr, size_t *length) const {
#if __riscv_xlen == 32
    using namespace ds2::Architecture::RISCV32;
#elif __riscv_xlen == 64
    using namespace ds2::Architecture::RISCV64;
#elif __riscv_xlen == 128
    using namespace ds2::Architecture::RISCV128;
#endif

    if (regno >= reg_lldb_x1 && regno <= reg_lldb_x31) {
      *ptr = const_cast<uintptr_t *>(&gp.regs[regno - reg_lldb_x1]);
      *length = sizeof(gp.regs[0]);
    } else if (regno == reg_lldb_pc) {
      *ptr = const_cast<uintptr_t *>(&gp.pc);
      *length = sizeof(gp.pc);
    } else if (regno >= reg_lldb_f0 && regno <= reg_lldb_f31) {
#if __riscv_flen == 32
      *ptr = const_cast<uint32_t *>(&fp.sng[regno - reg_lldb_f0]);
      *length = sizeof(fp.sng[0]);
#elif __riscv_flen == 64
      *ptr = const_cast<uint64_t *>(&fp.dbl[regno - reg_lldb_f0]);
      *length = sizeof(fp.dbl[0]);
#elif __riscv_flen == 128
#error "quad precision floating point support unimplemented"
#else
      return false;
#endif
    } else if (regno == reg_lldb_fcsr) {
      *ptr = const_cast<uint32_t *>(&fp.fcsr);
      *length = sizeof(fp.fcsr);
    } else {
      return false;
    }
    return true;
  }

  inline bool getGDBRegisterPtr(int regno, void **ptr, size_t *length) const {
#if __riscv_xlen == 32
    using namespace ds2::Architecture::RISCV32;
#elif __riscv_xlen == 64
    using namespace ds2::Architecture::RISCV64;
#elif __riscv_xlen == 128
    using namespace ds2::Architecture::RISCV128;
#endif

    if (regno >= reg_gdb_x1 && regno <= reg_gdb_x31) {
      *ptr = const_cast<uintptr_t *>(&gp.regs[regno - reg_gdb_x1]);
      *length = sizeof(gp.regs[0]);
    } else if (regno == reg_gdb_pc) {
      *ptr = const_cast<uintptr_t *>(&gp.pc);
      *length = sizeof(gp.pc);
    } else if (regno >= reg_gdb_f0 && regno <= reg_gdb_f31) {
#if __riscv_flen == 32
      *ptr = const_cast<uint32_t *>(&fp.sng[regno - reg_gdb_f0]);
      *length = sizeof(fp.sng[0]);
#elif __riscv_flen == 64
      *ptr = const_cast<uint64_t *>(&fp.dbl[regno - reg_gdb_f0]);
      *length = sizeof(fp.dbl[0]);
#elif __riscv_flen == 128
#error "quad precision floating point support unimplemented"
#else
      return false;
#endif
    } else if (regno == reg_gdb_fcsr) {
      *ptr = const_cast<uint32_t *>(&fp.fcsr);
      *length = sizeof(fp.fcsr);
    } else {
      return false;
    }
    return true;
  }
};

#pragma pack(pop)

} // namespace RISCV
} // namespace Architecture
} // namespace ds2
