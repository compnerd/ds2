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

#include "DebugServer2/Target/ThreadBase.h"

#include <vector>

namespace ds2 {
namespace Target {
namespace Windows {

class Thread : public ds2::Target::ThreadBase {
protected:
  HANDLE _handle;

protected:
  friend class Process;
  Thread(Process *process, ThreadId tid, HANDLE handle);

public:
  virtual ~Thread();

public:
  virtual ErrorCode terminate() override;

public:
  virtual ErrorCode suspend() override;

public:
  virtual ErrorCode step(int signal = 0,
                         Address const &address = Address()) override;
  virtual ErrorCode resume(int signal = 0,
                           Address const &address = Address()) override;

public:
  virtual ErrorCode afterResume();

public:
  virtual ErrorCode readCPUState(Architecture::CPUState &state) override;
  virtual ErrorCode writeCPUState(Architecture::CPUState const &state) override;

public:
  // The ExceptionCode from the most recent EXCEPTION_DEBUG_EVENT delivered
  // to this thread. On ARM64, there is no equivalent of x86's DR6 status
  // register to ask "did a debug register actually cause this stop", so
  // Sources/Core/Windows/ARM64/HardwareBreakpointManager.cpp's hit() uses
  // this (together with wasExplicitStep(), see below) as the next best
  // signal to rule out stops that cannot possibly be a watchpoint.
  DWORD lastExceptionCode() const { return _lastExceptionCode; }

#if defined(ARCH_ARM64)
public:
  // Reads/writes the hardware watchpoint value/control register pairs
  // (Wvr[]/Wcr[] in CONTEXT_DEBUG_REGISTERS). `addresses`/`controls` are
  // always sized to ARM64_MAX_WATCHPOINTS worth of entries by
  // readWatchpointRegisters().
  ErrorCode readWatchpointRegisters(std::vector<uint64_t> &addresses,
                                    std::vector<uint32_t> &controls);
  ErrorCode
  writeWatchpointRegisters(std::vector<uint64_t> const &addresses,
                           std::vector<uint32_t> const &controls);

public:
  // PSTATE.SS, per the ARMv8 architecture reference manual. Thread::step()
  // sets this bit before resuming to arm a single hardware step; the
  // architectural software-step exception transition clears it again
  // before the stopped context can be read (precisely so stepping does not
  // recursively re-trap), so it is not usable after the fact to tell
  // whether a stop was the completion of that step. Thread::afterResume()
  // also clears it, for the ordinary case where the step exception is
  // never taken (e.g. a breakpoint hit first).
  static constexpr uint64_t kPSTATE_SS = 1ULL << 21;

public:
  // Set by Thread::step() before it resumes with PSTATE.SS to arm a
  // hardware single step. updateState(DEBUG_EVENT const&) captures this
  // (and resets it) into wasExplicitStep() for the resulting stop, since
  // PSTATE.SS itself cannot be used for that (see above): this is the
  // ground truth for whether a STATUS_SINGLE_STEP is the expected
  // completion of a step this side requested, as opposed to an unrelated
  // watchpoint firing.
  void setExplicitStep() { _explicitStep = true; }
  bool wasExplicitStep() const { return _wasExplicitStep; }
#endif

private:
  DWORD _lastExceptionCode = 0;
#if defined(ARCH_ARM64)
  bool _explicitStep = false;
  bool _wasExplicitStep = false;
#endif

protected:
  virtual void updateState() override;
  virtual void updateState(DEBUG_EVENT const &de);
};
} // namespace Windows
} // namespace Target
} // namespace ds2
