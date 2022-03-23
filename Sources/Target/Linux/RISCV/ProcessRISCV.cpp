// Copyright 2022 Saleem Abdulrasool <compnerd@compnerd.org>

#include "DebugServer2/Target/Process.h"
#include "DebugServer2/Host/Platform.h"
#include "DebugServer2/Target/Thread.h"

#include <cmath>

#include <stddef.h>
#include <syscall.h>
#include <sys/mman.h>

namespace ds2 {
namespace Target {
namespace Linux {

namespace syscalls {
namespace {
template <typename T>
inline void InsertBytes(ByteVector &bytes, T value) {
  uint8_t *data = reinterpret_cast<uint8_t *>(&value);
  bytes.insert(std::end(bytes), data, data + sizeof(T));
}

inline void mmap(size_t size, int protection, ByteVector &code) {
#if __riscv_xlen == 32
  DS2BUG("do not know how to mmap on this bitness")
#elif __riscv_xlen == 64
  static_assert(sizeof(size) == 8, "size_t should be 8-bytes on RISCV64");
  DS2ASSERT(std::log2(MAP_ANON | MAP_PRIVATE) <= 20);
  DS2ASSERT(std::log2(__NR_mmap) <= 20);
  DS2ASSERT(std::log2(protection) <= 20);

  for (uint32_t instruction: {
           static_cast<uint32_t>(0x00004533),                                  // xor a0, zero, zero
           static_cast<uint32_t>(0x0245b583),                                  // ld a1, .Lsize
           static_cast<uint32_t>(0x0245b583),                                  //
           static_cast<uint32_t>(0x00000613 | protection << 20),               // li a2, protection
           static_cast<uint32_t>(0x00000693 | (MAP_ANON | MAP_PRIVATE) << 20), // li a3, MAP_ANON | MAP_PRIVATE
           static_cast<uint32_t>(0xfff00713),                                  // li a4, -1
           static_cast<uint32_t>(0x00000793),                                  // li a5, 0
           static_cast<uint32_t>(0x00000893 | __NR_mmap << 20),                // li a7, __NR_mmap
           static_cast<uint32_t>(0x00000073),                                  // ecall
           static_cast<uint32_t>(0x00100073),                                  // ebreak
                                                                               // .Lsize:
                                                                               //    .quad size
       })
    InsertBytes(code, instruction);
  InsertBytes(code, size);
#elif __riscv_xlen == 128
  DS2BUG("do not know how to mmap on this bitness")
#endif
}

inline void munmap(uintptr_t address, size_t size, ByteVector &code) {
#if __riscv_xlen == 32
  DS2BUG("do not know how to munmap on this bitness")
#elif __riscv_xlen == 64
  static_assert(sizeof(size) == 8, "size_t should be 8-bytes on RISCV64");
  DS2ASSERT(std::log2(__NR_munmap) <= 20);

  for (uint32_t instruction: {
           static_cast<uint32_t>(0x00000517),                                  // ld a0, .Laddress
           static_cast<uint32_t>(0x01c53503),                                  //
           static_cast<uint32_t>(0x00000597),                                  // ld a1, .Lsize
           static_cast<uint32_t>(0x01c5b583),                                  //
           static_cast<uint32_t>(0x00000893 | __NR_munmap << 20),              // li a7, __NR_munmap
           static_cast<uint32_t>(0x00000073),                                  // ecall
           static_cast<uint32_t>(0x00100073),                                  // ebreak
                                                                               // .Laddress:
                                                                               //    .quad address
                                                                               // .Lsize:
                                                                               //    .quad size
       })
    InsertBytes(code, instruction);
  InsertBytes(code, address);
  InsertBytes(code, size);
#elif __riscv_xlen == 128
  DS2BUG("do not know how to munmap on this bitness")
#endif
}
}
}

ErrorCode Process::allocateMemory(size_t size, uint32_t protection,
                                  uintptr_t *address) {
  if (address == nullptr || size == 0)
    return kErrorInvalidArgument;

  ByteVector code;
  syscalls::mmap(size, convertMemoryProtectionToPOSIX(protection), code);

  uintptr_t result;
  CHK(executeCode(code, result));
  if (static_cast<intptr_t>(result) < 0)
    return kErrorUnknown;
  *address = result;
  return kSuccess;
}

ErrorCode Process::deallocateMemory(uintptr_t address, size_t size) {
  if (size == 0)
    return kErrorInvalidArgument;

  ByteVector code;
  syscalls::munmap(address, size, code);

  uintptr_t result;
  CHK(executeCode(code, result));
  if (static_cast<intptr_t>(result) < 0)
    return kErrorUnknown;
  return kSuccess;
}

} // namespace Linux
} // namespace Target
} // namespace ds2
