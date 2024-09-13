//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Support/POSIX/ELFSupport.h"
#include "DebugServer2/Utils/Log.h"

#include <elf.h>
#include <fcntl.h>

namespace ds2 {
namespace Support {

bool ELFSupport::MachineTypeToCPUType(uint32_t machineType, bool is64Bit,
                                      CPUType &type, CPUSubType &subType) {
  switch (machineType) {
#if defined(ARCH_X86) || defined(ARCH_X86_64)
  case EM_386:
    type = kCPUTypeX86;
    subType = kCPUSubTypeX86_ALL;
    break;

#if defined(ARCH_X86_64)
  case EM_X86_64:
    type = kCPUTypeX86_64;
    subType = kCPUSubTypeX86_64_ALL;
    break;
#endif

#elif defined(ARCH_ARM) || defined(ARCH_ARM64)
  case EM_ARM:
    type = kCPUTypeARM;
    subType = kCPUSubTypeARM_ALL;
    break;

#if defined(ARCH_ARM64)
  case EM_AARCH64:
    type = kCPUTypeARM64;
    subType = kCPUSubTypeARM64_ALL;
    break;
#endif

#elif defined(ARCH_RISCV)
  case EM_RISCV:
#if __riscv_xlen == 32
    type = kCPUTypeRISCV32;
    subType = kCPUSubTypeRISCV32_ALL;
#elif __riscv_xlen == 64
    type = kCPUTypeRISCV64;
    subType = kCPUSubTypeRISCV64_ALL;
#elif __riscv_xlen == 128
    type = kCPUTypeRISCV128;
    subType = kCPUSubTypeRISCV128_ALL;
#endif
    break;

#else
#error "Architecture not supported."
#endif

  default:
    type = kCPUTypeAny;
    subType = kCPUSubTypeInvalid;
    return false;
  }
  return true;
}

bool ELFSupport::GetELFFileBuildID(std::string const &path, ByteVector &buildId) {
  int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0)
    return false;

  Elf32_Ehdr e32hdr;
  if (::pread(fd, &e32hdr, sizeof(e32hdr), 0) != sizeof(e32hdr) ||
      e32hdr.e_ident[EI_MAG0] != ELFMAG0 || e32hdr.e_ident[EI_MAG1] != ELFMAG1 ||
      e32hdr.e_ident[EI_MAG2] != ELFMAG2 || e32hdr.e_ident[EI_MAG3] != ELFMAG3) {
    DS2LOG(Warning, "File \"%s\" is not valid ELF format", path.c_str());
    ::close(fd);
    return false;
  }

  bool result = false;
  switch (e32hdr.e_ident[EI_CLASS]) {
    case ELFCLASS32: {
      Elf32_Shdr shdr;
      Elf32_Nhdr nhdr;
      result = ReadBuildID(fd, e32hdr, shdr, nhdr, buildId);
      break;
    }

    case ELFCLASS64:
      Elf64_Ehdr e64hdr;
      if (::pread(fd, &e64hdr, sizeof(e64hdr), 0) != sizeof(e64hdr))
        break;

      Elf64_Shdr s64hdr;
      Elf64_Nhdr n64hdr;
      result = ReadBuildID(fd, e64hdr, s64hdr, n64hdr, buildId);
      break;

    default:
      DS2LOG(Warning, "File \"%s\" contains unsupported ELF class identifier %i",
             path.c_str(), e32hdr.e_ident[EI_CLASS]);
      break;
  }

  ::close(fd);

  if (!result)
    DS2LOG(Warning, "Failed to query build ID for ELF file \"%s\"", path.c_str());

  return result;
}

template <typename ELFHeader, typename SectionHeader, typename NotesHeader>
bool ELFSupport::ReadBuildID(int fd, const ELFHeader &ehdr, SectionHeader &shdr,
                             NotesHeader &nhdr, ByteVector &id) {
    // Build ID is found in a note section with note type NT_GNU_BUILD_ID.
    // The section is typically named .note.gnu.build-id.
    for (size_t i = 0; i < ehdr.e_shnum; i++) {
      if (!ReadSectionHeader(fd, ehdr, shdr, i))
        return false;

      if (shdr.sh_type != SHT_NOTE)
        continue;

      if (::pread(fd, &nhdr, sizeof(nhdr), shdr.sh_offset) != sizeof(nhdr))
        return false;

      if (nhdr.n_type != NT_GNU_BUILD_ID)
        continue;

      id.resize(nhdr.n_descsz);
      const off_t pos = shdr.sh_offset + sizeof(nhdr) + nhdr.n_namesz;
      if (::pread(fd, id.data(), nhdr.n_descsz, pos) != nhdr.n_descsz)
        return false;

      return true;
    }

    return false;
}

template <typename ELFHeader, typename SectionHeader>
bool ELFSupport::ReadSectionHeader(int fd, const ELFHeader &ehdr, SectionHeader &shdr,
                                   size_t idx) {
  if (idx >= ehdr.e_shnum)
    return false;

  const off_t shdr_pos = ehdr.e_shoff + (idx * ehdr.e_shentsize);
  if (::pread(fd, &shdr, ehdr.e_shentsize, shdr_pos) != ehdr.e_shentsize)
    return false;

  return true;
}
} // namespace Support
} // namespace ds2
