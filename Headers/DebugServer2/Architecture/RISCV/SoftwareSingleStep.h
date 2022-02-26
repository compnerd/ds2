// Copyright 2022 Saleem Abdulrasool <compnerd@compnerd.org>

#pragma once

#include "DebugServer2/Architecture/CPUState.h"
#include "DebugServer2/Core/BreakpointManager.h"
#include "DebugServer2/Target/Process.h"

namespace ds2 {
namespace Architecture {
namespace RISCV {

ErrorCode PrepareSoftwareSingleStep(Target::Process *process,
                                    BreakpointManager *manager,
                                    CPUState const &state,
                                    Address const &address);

} // namespace RISCV
} // namespace Architecture
} // namespace ds2
