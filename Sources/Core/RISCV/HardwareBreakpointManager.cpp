// Copyright 2022 Saleem Abdulrasool <compnerd@compnerd.org>

#include "DebugServer2/Core/HardwareBreakpointManager.h"
#include "DebugServer2/Target/Process.h"
#include "DebugServer2/Target/Thread.h"

#define super ds2::BreakpointManager

namespace ds2 {

int HardwareBreakpointManager::hit(Target::Thread *thread, Site &site) {
  return -1;
};

ErrorCode HardwareBreakpointManager::enableLocation(Site const &site, int index,
                                                    Target::Thread *thread) {
  return kErrorUnsupported;
}

ErrorCode HardwareBreakpointManager::disableLocation(int index,
                                                     Target::Thread *thread) {
  return kErrorUnsupported;
}

size_t HardwareBreakpointManager::maxWatchpoints() {
  return _process->getMaxWatchpoints();
}

ErrorCode HardwareBreakpointManager::isValid(Address const &address,
                                             size_t size, Mode mode) const {
  return kErrorUnsupported;
}

size_t HardwareBreakpointManager::chooseBreakpointSize(Address const &) const {
  return -1;
}

} // namespace ds2
