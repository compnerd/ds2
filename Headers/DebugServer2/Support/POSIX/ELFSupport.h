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
  template <class T_EHDR, class T_SHDR, class T_NHDR>
  static bool ReadBuildID(int fd, const T_EHDR &ehdr, T_SHDR &shdr,
                          T_NHDR &nhdr, ByteVector &id);

  template <class T_EHDR, class T_SHDR>
  static bool ReadSectionHeader(int fd, const T_EHDR &ehdr, T_SHDR &shdr,
                                size_t idx);
};
} // namespace Support
} // namespace ds2
