//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#pragma once

#include "DebugServer2/Types.h"

namespace ds2 {
namespace Support {

class ELFSupport {
public:
  struct AuxiliaryVectorEntry {
    uint64_t type;
    uint64_t value;
  };

public:
  static bool MachineTypeToCPUType(uint32_t machineType, bool is64Bit,
                                   CPUType &type, CPUSubType &subType);
  static bool GetELFFileBuildID(std::string const &path, ByteVector &buildID);
private:
  template <typename ELFHeader, typename SectionHeader, typename NotesHeader>
  static bool ReadBuildID(int fd, const ELFHeader &ehdr, SectionHeader &shdr,
                          NotesHeader &nhdr, ByteVector &id);

  template <typename ELFHeader, typename SectionHeader>
  static bool ReadSectionHeader(int fd, const ELFHeader &ehdr,
                                SectionHeader &shdr, size_t idx);

  template <typename ELFHeader, typename SectionHeader>
  static bool ReadStringTable(int fd, const ELFHeader &ehdr,
                              SectionHeader &shdr, std::vector<char> &table);

  template <typename ELFHeader, typename SectionHeader>
  static bool ReadDebugLinkCRC(int fd, const ELFHeader &ehdr,
                               SectionHeader &shdr, ByteVector &crc);
};
} // namespace Support
} // namespace ds2
