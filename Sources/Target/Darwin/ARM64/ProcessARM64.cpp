// Copyright 2024-2025 Saleem Abdulrasool <compnerd@compnerd.org>

#include "DebugServer2/Target/Process.h"

#include <cmath>

#include <stddef.h>
#include <sys/mman.h>
#include <sys/syscall.h>

namespace {
template <typename T>
inline void InsertBytes(ds2::ByteVector &bytes, T value) {
  uint8_t *data = reinterpret_cast<uint8_t *>(&value);
  bytes.insert(std::end(bytes), data, data + sizeof(T));
}

namespace syscalls {
inline void mmap(size_t size, int protection, ds2::ByteVector &code) {
  DS2ASSERT(std::log2(MAP_ANON | MAP_PRIVATE) <= 16);
  DS2ASSERT(std::log2(SYS_mmap) <= 16);
  DS2ASSERT(std::log2(protection) <= 16);

  for (uint32_t instruction: {
            static_cast<uint32_t>(0xd2800000),                                  // mov x0, 0
            static_cast<uint32_t>(0x580000e1),                                  // ldr x1, .Lsize
            static_cast<uint32_t>(0xd2800002 | protection << 5),                // mov x2, protection
            static_cast<uint32_t>(0xd2800003 | (MAP_ANON | MAP_PRIVATE) << 5),  // mov x3, MAP_ANON | MAP_PRIVATE
            static_cast<uint32_t>(0x92800004),                                  // mov x4, -1
            static_cast<uint32_t>(0xd2800005),                                  // mov x5, 0
            static_cast<uint32_t>(0xd2800008 | SYS_mmap << 5),                  // mov x8, =SYS_mmap
            static_cast<uint32_t>(0xd4000001),                                  // svc 0
            static_cast<uint32_t>(0xd43e0000),                                  // brk #0xf000
                                                                                // .Lsize:
                                                                                //    .quad size
      })
    InsertBytes(code, instruction);
  InsertBytes(code, size);
}

inline void munmap(uintptr_t address, size_t size, ds2::ByteVector &code) {
  DS2ASSERT(std::log2(SYS_munmap) <= 16);

  for (uint32_t instruction: {
            static_cast<uint32_t>(0x580000a0),                                  // ldr x0, .Laddress
            static_cast<uint32_t>(0x580000c1),                                  // ldr x1, .Lsize
            static_cast<uint32_t>(0xd2800008 | SYS_munmap << 5),                // mov x8, =SYS_munmap
            static_cast<uint32_t>(0xd4000001),                                  // svc 0
            static_cast<uint32_t>(0xd43e0000),                                  // brk #0xf000
                                                                                // .Laddress:
                                                                                //    .quad address
                                                                                // .Lsize:
                                                                                //    .quad size
      })
    InsertBytes(code, instruction);
  InsertBytes(code, address);
  InsertBytes(code, size);
}
}
}

namespace ds2 {
namespace Target {
namespace Darwin {
ErrorCode Process::allocateMemory(size_t size, uint32_t protection,
                                  uint64_t *address) {
  if (address == nullptr || size == 0)
    return kErrorInvalidArgument;

  ProcessInfo info;
  ErrorCode error = getInfo(info);
  if (error != kSuccess)
    return error;

  ByteVector code;
  syscalls::mmap(size, convertMemoryProtectionFromPOSIX(protection), code);

  error = ptrace().execute(_pid, info, &code[0], code.size(), *address);
  if (error != kSuccess)
    return error;

  if (*address == reinterpret_cast<uint64_t>(MAP_FAILED))
    return kErrorNoMemory;
  return kSuccess;
}

ErrorCode Process::deallocateMemory(uint64_t address, size_t size) {
  if (size == 0)
    return kErrorInvalidArgument;

  ProcessInfo info;
  ErrorCode error = getInfo(info);
  if (error != kSuccess)
    return error;

  ByteVector code;
  syscalls::munmap(address, size, code);

  uint64_t result = 0;
  error = ptrace().execute(_pid, info, &code[0], code.size(), result);
  if (error != kSuccess)
    return error;

  if (static_cast<int64_t>(result) < 0)
    return kErrorInvalidArgument;
  return kSuccess;
}
}
}
}
