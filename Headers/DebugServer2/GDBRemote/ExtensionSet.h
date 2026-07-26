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

#include <cstdint>
#include <string>

namespace ds2 {
namespace GDBProtocol {

class ExtensionSet {
public:
  struct AdvertisedFeature {
    char const *name;
    bool supported;
  };

  enum Extension : uint32_t {
    kQEcho = (1u << 0),
    kQStartNoAckMode = (1u << 1),
    kQXferFeaturesRead = (1u << 2),
    kQXferAuxvRead = (1u << 3),
    kQXferLibrariesSVR4Read = (1u << 4),
    kQXferLibrariesRead = (1u << 5),
    kQListThreadsInStopReply = (1u << 6),
    kQPassSignals = (1u << 7),
    kBreakpointCommands = (1u << 8),
    kMultiprocess = (1u << 9),
    kQDisableRandomization = (1u << 10),
    kQNonStop = (1u << 11),
    kQProgramSignals = (1u << 12),
    kQXferSiginfoRead = (1u << 13),
    kQXferSiginfoWrite = (1u << 14),
    kForkEvents = (1u << 15),
    kVForkEvents = (1u << 16),
    kQXferOSDataRead = (1u << 17),
    kQXferThreadsRead = (1u << 18),
  };

public:
  void reset() {
    _requested = 0;
    _supported = 0;
    _enabled = 0;
  }

  void enable(Extension extension) {
    _supported |= extension;
    _enabled = _requested & _supported;
  }

  void disable(Extension extension) {
    _supported &= ~static_cast<uint32_t>(extension);
    _enabled = _requested & _supported;
  }

  void negotiate(std::string const &feature) {
    Extension extension;
    if (!find(feature, extension))
      return;

    _requested |= extension;
    _enabled = _requested & _supported;
  }

  bool enabled(Extension extension) const {
    return (_enabled & extension) != 0;
  }

  bool supported(Extension extension) const {
    return (_supported & extension) != 0;
  }

  std::string feature(Extension extension) const {
    return feature(extension, supported(extension));
  }

  std::string feature(Extension extension, bool state) const {
    char const *featureName = this->name(extension);
    if (featureName == nullptr)
      return std::string();

    std::string feature(featureName);
    feature.push_back(state ? '+' : '-');
    return feature;
  }

  AdvertisedFeature advertise(Extension extension,
                              bool advertiseWhenUnsupported = false) const {
    bool isSupported = supported(extension);
    char const *featureName = name(extension);
    if (featureName == nullptr || (!isSupported && !advertiseWhenUnsupported))
      return {nullptr, false};

    return {featureName, isSupported};
  }

  char const *name(Extension extension) const {
    for (auto const &entry : kEntries) {
      if (entry.extension == extension)
        return entry.name;
    }

    return nullptr;
  }

private:
  bool find(std::string const &feature, Extension &extension) const {
    for (auto const &entry : kEntries) {
      if (feature == entry.name) {
        extension = entry.extension;
        return true;
      }
    }

    return false;
  }

private:
  struct Entry {
    Extension extension;
    char const *name;
  };

  static constexpr Entry kEntries[] = {
      {kQEcho, "qEcho"},
      {kQStartNoAckMode, "QStartNoAckMode"},
      {kQXferFeaturesRead, "qXfer:features:read"},
      {kQXferAuxvRead, "qXfer:auxv:read"},
      {kQXferLibrariesSVR4Read, "qXfer:libraries-svr4:read"},
      {kQXferLibrariesRead, "qXfer:libraries:read"},
      {kQListThreadsInStopReply, "QListThreadsInStopReply"},
      {kQPassSignals, "QPassSignals"},
      {kBreakpointCommands, "BreakpointCommands"},
      {kMultiprocess, "multiprocess"},
      {kQDisableRandomization, "QDisableRandomization"},
      {kQNonStop, "QNonStop"},
      {kQProgramSignals, "QProgramSignals"},
      {kQXferSiginfoRead, "qXfer:siginfo:read"},
      {kQXferSiginfoWrite, "qXfer:siginfo:write"},
      {kForkEvents, "fork-events"},
      {kVForkEvents, "vfork-events"},
      {kQXferOSDataRead, "qXfer:osdata:read"},
      {kQXferThreadsRead, "qXfer:threads:read"},
  };

private:
  uint32_t _requested = 0;
  uint32_t _supported = 0;
  uint32_t _enabled = 0;
};

} // namespace GDBProtocol
} // namespace ds2