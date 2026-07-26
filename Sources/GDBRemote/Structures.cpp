//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/GDBRemote/ProtocolHelpers.h"
#include "DebugServer2/GDBRemote/Types.h"
#include "DebugServer2/Utils/HexValues.h"
#include "DebugServer2/Utils/Log.h"
#include "DebugServer2/Utils/String.h"
#include "DebugServer2/Utils/Stringify.h"
#include "DebugServer2/Utils/SwapEndian.h"
#include "JSObjects/JSObjects.h"

#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>

#if defined(OS_POSIX)
#define FORMAT_ID(ID) ID
#elif defined(OS_WIN32)
#define FORMAT_ID(ID) 0
#else
#error "Target not supported."
#endif

using ds2::Utils::Stringify;

namespace ds2 {
namespace GDBRemote {

namespace {

const char *EndianName(Endian endian) {
  switch (endian) {
  case kEndianBig:
    return "big";
  case kEndianLittle:
    return "little";
  case kEndianPDP:
    return "pdp";
  default:
    return "unknown";
  }
}

std::string JoinRegisterNumbers(const std::vector<uint32_t> &registers,
                                bool decimal) {
  std::ostringstream ss;
  for (size_t n = 0; n < registers.size(); ++n) {
    if (n != 0)
      ss << ',';
    ss << (decimal ? std::dec : std::hex) << registers[n] << std::dec;
  }
  return ss.str();
}

} // namespace

//
// feature+ or feature- or feature? or feature=value
//
bool Feature::parse(std::string const &string) {
  size_t pos = string.find_last_of("?+-=");
  if (pos == std::string::npos)
    return false;

  name = string.substr(0, pos);
  switch (string[pos]) {
  case '?':
    flag = kQuerySupported;
    break;
  case '+':
    flag = kSupported;
    break;
  case '-':
    flag = kNotSupported;
    break;
  case '=':
    flag = kSupported;
    value = string.substr(pos + 1);
    break;
  }

  return true;
}

#define HEX0 std::hex << std::setfill('0')
#define HEX(N) HEX0 << std::setw(N)
#define DEC std::dec

//
// GDB and LLDB differs in encoding the thread suffix,
// there are a total of three encodings:
//    <pid>              - GDB w/o multiprocess support
//    <tid>              - LLDB default mode
//    p<pid>.<tid>       - GDB w/ multiprocess support
//    <pid>;thread:<tid> - LLDB w/ thread suffix support
//

bool ProcessThreadId::parse(std::string const &string, CompatibilityMode mode) {
  this->pid = kAllProcessId;
  this->tid = kAllThreadId;

  if (string.empty())
    return false;

  unsigned long new_pid = kAllProcessId;
  unsigned long new_tid = kAllThreadId;

  const char *str = string.c_str();
  char *eptr = nullptr;

  auto parse_hex = [&eptr](const char *str, unsigned long &value) -> bool {
    errno = 0;
    value = std::strtoul(str, &eptr, 16);
    return str != eptr && errno == 0;
  };

  switch (mode) {
  case kCompatibilityModeGDB:
  case kCompatibilityModeGDBMultiprocess:
    if (*str == 'p') {
      str += 1;
      if (!parse_hex(str, new_pid))
        return false;

      if (*eptr++ == '.') {
        str = eptr;
        if (!parse_hex(str, new_tid))
          return false;
      }
    } else {
      if (!parse_hex(str, new_pid))
        return false;
    }
    break;

  case kCompatibilityModeLLDB:
    if (!parse_hex(str, new_pid))
      return false;

    if (*eptr++ == ';') {
      if (std::strncmp(eptr, "thread:", 7) == 0) {
        str = eptr + 7;
        if (!parse_hex(str, new_tid))
          return false;
      }
    } else {
      new_tid = new_pid;
      new_pid = kAnyProcessId;
    }
    break;

  case kCompatibilityModeLLDBThread:
    if (std::strncmp(str, "thread:", 7) != 0)
      return false;

    str += 7;
    if (!std::isxdigit(static_cast<unsigned char>(*str)))
      return false;

    if (!parse_hex(str, new_tid))
      return false;

    // LLDB can terminate the thread suffix with a trailing ';'.
    if (*eptr == ';')
      ++eptr;

    if (*eptr != '\0')
      return false;
    break;

  default:
    return false;
  }

  this->pid = new_pid;
  this->tid = new_tid;
  return true;
}

std::string ProcessThreadId::encode(CompatibilityMode mode) const {
  std::ostringstream ss;

  if (mode == kCompatibilityModeGDB) {
    ss << HEX0 << pid;
  } else if (mode == kCompatibilityModeGDBMultiprocess) {
    if (validTid()) {
      ss << 'p';
    }

    ss << HEX0 << pid;
    if (validTid()) {
      ss << '.' << HEX0 << tid;
    }
  } else if (mode == kCompatibilityModeLLDB ||
             mode == kCompatibilityModeLLDBThread) {
    if (mode == kCompatibilityModeLLDBThread) {
      if (!validTid()) {
        ss << HEX0 << pid;
      } else {
        ss << HEX0 << tid;
      }
    } else {
      ss << HEX0 << pid;
      if (validTid()) {
        ss << ';' << "thread:" << HEX0 << tid;
      }
    }
  }

  return ss.str();
}

void StopInfo::getWatchpointInfo(std::string &key, std::string &val,
                                 CompatibilityMode mode, bool encodeHex) const {
  std::ostringstream ss;

  if (mode == kCompatibilityModeLLDB) {
    key = "description";
    ss << watchpointAddress << " " << watchpointIndex;
  } else {
    switch (reason) {
    case StopInfo::kReasonWriteWatchpoint:
      key = "watch";
      break;
    case StopInfo::kReasonReadWatchpoint:
      key = "rwatch";
      break;
    case StopInfo::kReasonAccessWatchpoint:
      key = "awatch";
      break;
    default:
      DS2BUG("Watchpoint stop reason invalid");
    }
    ss << std::hex << watchpointAddress;
  }

  val = encodeHex ? ToHex(ss.str()) : ss.str();
}

void StopInfo::reasonToString(std::string &key, std::string &val,
                              CompatibilityMode mode) const {
  key = "reason";

  switch (reason) {
  case StopInfo::kReasonTrace:
    val = "trace";
    break;
  case StopInfo::kReasonBreakpoint:
    val = "breakpoint";
    break;
  case StopInfo::kReasonSignalStop:
    val = "signal";
    break;
  case StopInfo::kReasonTrap:
    val = "trap";
    break;
  case StopInfo::kReasonFork:
    val = "fork";
    break;
  case StopInfo::kReasonVFork:
    val = "vfork";
    break;
  case StopInfo::kReasonVForkDone:
    val = "vforkdone";
    break;
  case StopInfo::kReasonWriteWatchpoint:
  case StopInfo::kReasonReadWatchpoint:
  case StopInfo::kReasonAccessWatchpoint:
    if (mode == kCompatibilityModeLLDB) {
      val = "watchpoint";
    } else {
      key = "";
      val = "";
    }
    break;
#if defined(OS_WIN32)
  case StopInfo::kReasonLibraryEvent:
    key = "library";
    val = "1";
    break;
#endif
  default:
    key = "";
    val = "";
    break;
  }
}

std::string StopInfo::encodeInfo(CompatibilityMode mode,
                                 bool listThreads) const {
  std::ostringstream ss;

  CompatibilityMode threadMode =
      (mode == kCompatibilityModeLLDB) ? kCompatibilityModeLLDBThread : mode;

  ss << "thread:" << ptid.encode(threadMode);
  if (!threadName.empty()) {
    ss << ';' << "name:" << threadName;
  }
  if (core >= 0) {
    ss << ';' << "core:" << core;
  }

  std::string key, val;
  reasonToString(key, val, mode);

  if (!key.empty() && !val.empty()) {
    ss << ';' << key << ':' << val;
  }

  if (watchpointAddress) {
    getWatchpointInfo(key, val, mode, mode == kCompatibilityModeLLDB);
    ss << ';' << key << ':' << val;
  }

  if (reason == StopInfo::kReasonSignalStop) {
    ss << ';' << "signal:" << signal;

    // The stop description conventionally carries the fault address as an
    // "address=<hex>" substring, so a client can locate the faulting
    // expression; emit 0x so base-auto parsers decode the full value.
    if (fault.has_value()) {
      std::ostringstream desc;
      desc << "address=0x" << std::hex
           << static_cast<uint64_t>(fault.value());
      ss << ';' << "description:" << ToHex(desc.str());
    }
  }

  if (reason == StopInfo::kReasonFork || reason == StopInfo::kReasonVFork) {
    // The fork-events/vfork-events extension reports the forked child using
    // the same GDB multiprocess-style "p<pid>.<tid>" format ProcessThreadId
    // ::encode() already produces for kCompatibilityModeGDBMultiprocess.
    auto [processId, threadId] = child;
    ss << ';' << (reason == StopInfo::kReasonFork ? "fork" : "vfork") << ":p"
       << HEX0 << static_cast<uint64_t>(processId) << '.' << HEX0
       << static_cast<uint64_t>(threadId) << DEC;
  }

  if (reason == StopInfo::kReasonVForkDone) {
    ss << ';' << "vforkdone:";
  }

  if (listThreads) {
    ss << ';' << "threads:";
    if (threads.empty()) {
      //
      // Best effort, send only this thread.
      //
      ss << ptid.encode(threadMode);
    } else {
      bool first = true;
      for (auto &tid : threads) {
        if (!first) {
          ss << ',';
        }
        ss << HEX0 << tid;
        first = false;
      }
    }
  }

  return ss.str();
}

std::string StopInfo::formatRegisterNumber(uint64_t regno, bool hexIndex) const {
  std::ostringstream ss;
  if (hexIndex) {
    ss << HEX(2) << (regno & 0xff);
  } else {
    ss << DEC << regno;
  }
  return ss.str();
}

std::string
StopInfo::formatRegisterValue(const Architecture::GPRegisterValue &value) const {
  std::ostringstream ss;
  size_t regsize = value.size << 3;
#if defined(ENDIAN_BIG)
  ss << HEX(regsize >> 2) << value.value;
#else
  ss << HEX(regsize >> 2) << (Swap64(value.value) >> (64 - regsize));
#endif
  return ss.str();
}

std::string StopInfo::encodeRegisters() const {
  std::ostringstream ss;
  bool first = true;

  for (const auto &reg : registers) {
    if (!first) {
      ss << ';';
    }

    ss << formatRegisterNumber(reg.first, true) << ':'
       << formatRegisterValue(reg.second);

    first = false;
  }

  return ss.str();
}

std::string StopInfo::encode(CompatibilityMode mode, bool listThreads) const {
  // We shouldn't be trying to encode something that has no stop event.
  DS2ASSERT(event != kEventNone);

  std::ostringstream ss;
  if (event == kEventStop && mode == kCompatibilityModeGDBMultiprocess) {
    //
    // We need to have some information in order to
    // have extended stop reason.
    //
    if (!ptid.valid() && core < 0 && reason == StopInfo::kReasonNone &&
        registers.empty()) {
      //
      // We can use the simpler form.
      //
      mode = kCompatibilityModeGDB;
    }
  }

  switch (event) {
  case kEventStop:
    ss << ((mode != kCompatibilityModeGDB) ? 'T' : 'S') << HEX(2);
#if !defined(OS_WIN32)
    ss << ((reason != StopInfo::kReasonNone) ? (signal & 0xff) : 0);
#else
    // Windows doesn't have a notion of signals but the GDB protocol still
    // needs some sort of emulation for these.
    switch (reason) {
    case StopInfo::kReasonNone:
    case StopInfo::kReasonLibraryEvent:
      ss << 0;
      break;
    case StopInfo::kReasonBreakpoint:
    case StopInfo::kReasonAccessWatchpoint:
    case StopInfo::kReasonReadWatchpoint:
    case StopInfo::kReasonWriteWatchpoint:
    case StopInfo::kReasonTrace:
      ss << 5; // SIGTRAP
      break;
    case StopInfo::kReasonMemoryError:
      ss << 11; // SIGSEGV
      break;
    case StopInfo::kReasonMemoryAlignment:
      ss << 7; // SIGBUS
      break;
    case StopInfo::kReasonMathError:
      ss << 8; // SIGFPE
      break;
    case StopInfo::kReasonInstructionError:
      ss << 4; // SIGILL
      break;
    case StopInfo::kReasonUserException:
      ss << 30; // SIGUSR1
      break;
    default:
      DS2BUG("not implemented");
    }
#endif
    ss << DEC;
    break;

  case kEventExit:
    ss << 'W' << HEX(2) << (status & 0xff) << DEC;
    break;

  case kEventKill:
    ss << 'X' << HEX(2);
#if !defined(OS_WIN32)
    ss << (signal & 0xff);
#else
    ss << 9; // SIGKILL
#endif
    ss << DEC;
    break;

  default:
    DS2BUG("impossible StopInfo event: %s", Stringify::StopEvent(event));
  }

  //
  // When sending signals, LLDB expects that thread information
  // is present at the beginning, followed by the registers, while
  // GDB expects registers first.
  //
  if (event == kEventStop && mode != kCompatibilityModeGDB) {
    if (mode == kCompatibilityModeLLDB) {
      ss << encodeInfo(mode, listThreads) << ';' << encodeRegisters();
    } else {
      ss << encodeRegisters() << ';' << encodeInfo(mode, listThreads);
    }

    ss << ';';
  }
  return ss.str();
}

std::string
StopInfo::encodeWithAllThreads(CompatibilityMode mode,
                               const JSArray &threadsStopInfo) const {
  std::ostringstream ss;
  ss << encode(mode, true) << "jstopinfo:" << ToHex(threadsStopInfo.toString())
     << ";";
  return ss.str();
}

JSDictionary *StopInfo::encodeJson() const {
  auto threadObj = JSDictionary::New();

  threadObj->set("tid", JSInteger::New(ptid.tid));

  std::string key, val;
  reasonToString(key, val, kCompatibilityModeLLDB);
  if (!key.empty() && !val.empty()) {
    threadObj->set(key, JSString::New(val));
  }

  if (!threadName.empty())
    threadObj->set("name", JSString::New(threadName));

  if (core)
    threadObj->set("core", JSInteger::New(core));

  if (watchpointAddress) {
    std::string watchpointKey, watchpointVal;
    getWatchpointInfo(watchpointKey, watchpointVal, kCompatibilityModeLLDB,
                      false);
    threadObj->set(watchpointKey, JSString::New(watchpointVal));
  }

  if (reason == StopInfo::kReasonSignalStop) {
    threadObj->set("signal", JSInteger::New(signal));
  }

  auto regSet = JSDictionary::New();
  for (const auto &reg : registers) {
    regSet->set(formatRegisterNumber(reg.first, false),
                JSString::New(formatRegisterValue(reg.second)));
  }

  threadObj->set("registers", regSet);

  return threadObj;
}

std::string HostInfo::encode() const {
  std::ostringstream ss;

  //
  // For non-Apple platforms we will send arch: for qHostInfo
  // encoding, this because LLDB will assume a Mach-O target
  // (and thus only iOS and MacOS X) in case cputype: and
  // cpusubtype: are specified; however qProcessInfo requires
  // them.
  //

#if defined(__APPLE__)
  bool sForceCPUType = true;
#else
  bool sForceCPUType = false;
#endif

  if (sForceCPUType) {
    ss << "cputype:" << DEC << cpuType << ';';
    if (cpuSubType != 0) {
      ss << "cpusubtype:" << DEC << cpuSubType << ';';
    }
  } else {
    ss << "arch:" << GetArchName(cpuType, cpuSubType, endian) << ';';
  }

  ss << "ostype:" << osType << ';';
  if (!osVendor.empty()) {
    ss << "vendor:" << osVendor << ';';
  }
  if (!osBuild.empty()) {
    ss << "os_build:" << ToHex(osBuild) << ';';
  }
  if (!osKernel.empty()) {
    ss << "os_kernel:" << ToHex(osKernel) << ';';
  }
  if (!osVersion.empty()) {
    unsigned int major, minor, revision;

    major = minor = revision = 0;

    //
    // Parse to ensure the format is maj.min.rev.
    //
    if (std::sscanf(osVersion.c_str(), "%u.%u.%u", &major, &minor, &revision) >
        0) {
      ss << "os_version:" << DEC << major << '.' << DEC << minor << '.' << DEC
         << revision << ';';
    }
  }
  if (!hostName.empty()) {
    ss << "hostname:" << ToHex(hostName) << ';';
  }
  ss << "endian:" << EndianName(endian) << ';';
  ss << "ptrsize:" << pointerSize << ';';
  ss << "watchpoint_exceptions_received:"
     << (watchpointExceptionsReceivedBefore ? "before" : "after") << ';';

  if (defaultPacketTimeout > 0)
    ss << "default_packet_timeout:" << defaultPacketTimeout << ';';

  return ss.str();
}

std::string ProcessInfo::encode(CompatibilityMode mode,
                                bool alternateVersion) const {
  std::ostringstream ss;

  std::string triple;

  if (mode == kCompatibilityModeLLDB || alternateVersion) {
    triple = GetArchName(cpuType, cpuSubType);
    triple += '-';
    if (osVendor.empty()) {
      triple += "unknown";
    } else {
      triple += osVendor;
    }
    triple += '-';
    if (osType.empty()) {
      triple += "unknown";
    } else {
      triple += osType;
    }
  }

  if (alternateVersion) {
    ss << "pid:" << DEC << pid << ';';
    ss << "uid:" << DEC << FORMAT_ID(realUid) << ';';
    ss << "gid:" << DEC << FORMAT_ID(realGid) << ';';
#if !defined(OS_WIN32)
    ss << "ppid:" << DEC << parentPid << ';';
    ss << "euid:" << DEC << effectiveUid << ';';
    ss << "egid:" << DEC << effectiveGid << ';';
#endif
    ss << "name:" << ToHex(name) << ';';
    ss << "triple:" << ToHex(triple) << ';';
  } else {
    ss << "pid:" << HEX0 << pid << ';';
    ss << "real-uid:" << HEX0 << FORMAT_ID(realUid) << ';';
    ss << "real-gid:" << HEX0 << FORMAT_ID(realGid) << ';';
#if !defined(OS_WIN32)
    ss << "parent-pid:" << HEX0 << parentPid << ';';
    ss << "effective-uid:" << HEX0 << effectiveUid << ';';
    ss << "effective-gid:" << HEX0 << effectiveGid << ';';
#endif
    if (mode == kCompatibilityModeLLDB) {
      ss << "triple:" << ToHex(triple) << ';';
    } else {
      // CPU{,Sub}Type contains an `enum CPUType`, and nativeCPU{,Sub}Type
      // contains the actual value that will be sent on the wire (e.g.: for ELF
      // processes it would contain values from the ELF header).
      ss << "cputype:" << HEX0 << nativeCPUType << ';';
      if (nativeCPUSubType != 0) {
        ss << "cpusubtype:" << HEX0 << nativeCPUSubType << ';';
      }
    }
    ss << "endian:" << EndianName(endian) << ';';
    ss << "ptrsize:" << pointerSize << ';';
    ss << "vendor:" << osVendor << ";";
    ss << "ostype:" << osType << ";";
  }

  return ss.str();
}

std::string RegisterInfo::encode(int xmlSet) const {
  bool xml = (xmlSet >= 0);

  char const *encodingName;
  switch (encoding) {
  case kEncodingNone:
    encodingName = nullptr;
    break;
  case kEncodingUInt:
    encodingName = "uint";
    break;
  case kEncodingSInt:
    encodingName = "sint";
    break;
  case kEncodingIEEE754:
    encodingName = "ieee754";
    break;
  case kEncodingVector:
    encodingName = "vector";
    break;
  default:
    return std::string();
  }

  char const *formatName;
  switch (format) {
  case kFormatNone:
    formatName = nullptr;
    break;
  case kFormatBinary:
    formatName = "binary";
    break;
  case kFormatDecimal:
    formatName = "decimal";
    break;
  case kFormatHex:
    formatName = "hex";
    break;
  case kFormatFloat:
    formatName = "float";
    break;
  case kFormatVectorUInt8:
    formatName = "vector-uint8";
    break;
  case kFormatVectorSInt8:
    formatName = "vector-sint8";
    break;
  case kFormatVectorUInt16:
    formatName = "vector-uint16";
    break;
  case kFormatVectorSInt16:
    formatName = "vector-sint16";
    break;
  case kFormatVectorUInt32:
    formatName = "vector-uint32";
    break;
  case kFormatVectorSInt32:
    formatName = "vector-sint32";
    break;
  case kFormatVectorUInt128:
    formatName = "vector-uint128";
    break;
  case kFormatVectorFloat32:
    formatName = "vector-float32";
    break;
  default:
    return std::string();
  }

  std::ostringstream ss;
  if (xml) {
    ss << "<reg ";

    auto append = [&ss](char const *key, auto const &value) {
      ss << key << "=" << '"' << value << '"' << ' ';
    };

    append("name", registerName);
    if (!alternateName.empty())
      append("altname", alternateName);

    append("bitsize", bitSize);
    append("offset", byteOffset < 0 ? 0 : byteOffset);

    if (encodingName != nullptr)
      append("encoding", encodingName);

    if (formatName != nullptr)
      append("format", formatName);

    if (!setName.empty())
      append("group_id", xmlSet);

    append("regnum", regno);

    if (ehframeRegisterIndex >= 0)
      append("ehframe_regnum", ehframeRegisterIndex);

    if (dwarfRegisterIndex >= 0)
      append("dwarf_regnum", dwarfRegisterIndex);

    if (!genericName.empty())
      append("generic", genericName);

    if (!containerRegisters.empty())
      append("value_regnums", JoinRegisterNumbers(containerRegisters, true));

    if (!invalidateRegisters.empty())
      append("invalidate_regnums", JoinRegisterNumbers(invalidateRegisters, true));

    ss << "/>";
  } else {
    auto append = [&ss](char const *key, auto const &value) {
      ss << key << ":" << value << ";";
    };

    append("name", registerName);
    if (!alternateName.empty())
      append("alt-name", alternateName);

    append("bitsize", bitSize);
    append("offset", byteOffset < 0 ? 0 : byteOffset);

    if (encodingName != nullptr)
      append("encoding", encodingName);

    if (formatName != nullptr)
      append("format", formatName);

    if (!setName.empty())
      append("set", setName);

    if (ehframeRegisterIndex >= 0)
      append("ehframe", ehframeRegisterIndex);

    if (dwarfRegisterIndex >= 0)
      append("dwarf", dwarfRegisterIndex);

    if (!genericName.empty())
      append("generic", genericName);

    if (!containerRegisters.empty())
      append("container-regs", JoinRegisterNumbers(containerRegisters, false));

    if (!invalidateRegisters.empty())
      append("invalidate-regs", JoinRegisterNumbers(invalidateRegisters, false));
  }

  return ss.str();
}

std::string MemoryRegionInfo::encode() const {
  std::ostringstream ss;

  ss << "start:" << HEX(8) << start << DEC << ';';
  ss << "size:" << HEX(8) << length << DEC << ';';
  if (protection != 0) {
    ss << "permissions:";
    if (protection & kProtectionRead)
      ss << 'r';
    if (protection & kProtectionWrite)
      ss << 'w';
    if (protection & kProtectionExecute)
      ss << 'x';
    ss << ';';
  }
  if (!name.empty()) {
    ss << "name:" << ToHex(name) << ';';
  }

  return ss.str();
}

std::string ServerVersion::encode() const {
  std::ostringstream ss;
  ss << "name:" << name << ';';
  if (!version.empty()) {
    ss << "version:" << version << ';';
  }
  if (!patchLevel.empty()) {
    ss << "patch_level:" << patchLevel << ';';
  }
  if (!releaseName.empty()) {
    ss << "release_name:" << releaseName << ';';
  }
  ss << "build_number:" << buildNumber << ';'
     << "major_version:" << majorVersion << ';'
     << "minor_version:" << majorVersion << ';';

  return ss.str();
}

std::string ProgramResult::encode() const {
  // F,exitcode,signal,escaped-binary-data
  std::ostringstream ss;
  ss << 'F' << ',' << HEX(8) << status << DEC << ',' << HEX(8) << signal << DEC
     << ',' << Escape(output);
  return ss.str();
}

std::string ModuleInfo::encode() const {
  std::ostringstream ss;
  if (!uuid.empty())
    ss << "uuid:" << ToHex(uuid) << ";";
  ss << "triple:" << ToHex(triple) << ";";
  ss << "file_path:" << ToHex(file_path) << ";";
  ss << "file_offset:" << HEX(16) << file_offset << ";";
  ss << "file_size:" << HEX(16) << file_size << ";";
  return ss.str();
}
} // namespace GDBRemote
} // namespace ds2
