//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/GDBRemote/ProtocolInterpreter.h"
#include "DebugServer2/GDBRemote/ProtocolHelpers.h"
#include "DebugServer2/GDBRemote/Session.h"
#include "DebugServer2/Utils/Log.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>

namespace ds2 {
namespace GDBRemote {

namespace {

std::string EscapeForTerm(std::string const &s) {
  std::ostringstream ss;
  for (char n : s) {
    unsigned c = static_cast<unsigned>(n & 0xff);
    if (c < 0x20 || c > 0x7f) {
      ss << "\\x" << std::hex << std::setw(2) << std::setfill('0') << c
         << std::dec;
    } else {
      ss << n;
    }
  }
  return ss.str();
}

using CommandRange = std::pair<size_t, size_t>;

CommandRange splitCommand(const std::string &data) {
  CommandRange range = {std::string::npos, std::string::npos};

  if (data.empty())
    return range;

  if (data[0] == 'v' || data[0] == 'q' || data[0] == 'Q') {
    //
    // Commands starting with 'v', 'q', or 'Q' are terminated by
    // one of: ',' (comma), ':' (colon), or ';' (semi-colon).
    //
    size_t end = data.find_first_of(",:;");
    if (end != std::string::npos) {
      range.first = end;
      range.second = end + 1;
    }
  } else if (data[0] == 'b') {
    //
    // Commands starting with 'b' may be two chars long; only 'bc'
    // and 'bs' are known.
    //
    range.first = (data.length() == 2 && (data[1] == 'c' || data[1] == 's'))
                      ? 2
                      : 1;
  } else if (data[0] == '_') {
    //
    // Commands starting with '_' may be two chars long; only '_M'
    // and '_m' are known.
    //
    range.first = (data.length() > 1 && (data[1] == 'M' || data[1] == 'm'))
                      ? 2
                      : 1;
  } else if (data[0] == 'j') {
    //
    // Commands starting with 'j' are terminated by ':' (colon).
    //
    size_t end = data.find_first_of(":");
    if (end != std::string::npos) {
      range.first = end;
      range.second = end + 1;
    }
  } else {
    //
    // Any other command is one character.
    //
    range.first = 1;
  }

  if (range.second == std::string::npos && range.first < data.length())
    range.second = range.first;

  return range;
}

} // namespace

ProtocolInterpreter::ProtocolInterpreter() : _session(nullptr) {}

void ProtocolInterpreter::onPacketData(std::string const &data, bool valid) {
  DS2LOG(Packet, "getpkt(\"%s\")", EscapeForTerm(&data[0]).c_str());

  if (_session == nullptr)
    return;

  if (data.length() == 1) {
    //
    // ACK and NAKs are handled specially.
    //
    switch (data[0]) {
    case '+':
      _session->onACK();
      return;

    case '-':
      _session->onNAK();
      return;

    default:
      break;
    }
  }

  //
  // Inform the session that we received a command, if it's not
  // valid, the session may resend the previous command or send
  // an ack or a nak.
  //
  if (!_session->onCommandReceived(valid) || !valid)
    return;

  //
  // Extract the command and arguments to pass down to the
  // handler.
  //
  CommandRange range = splitCommand(data);

  std::string_view command(data.data(), range.first);
  std::string_view args;
  if (range.second != std::string::npos)
    args = std::string_view(data.data() + range.second,
                            data.length() - range.second);

  //
  // Find the handler and execute it.
  //
  onCommand(command, args);
}

void ProtocolInterpreter::onInvalidData(std::string const &data) {
  DS2LOG(Warning, "received invalid data: '%s'", data.c_str());

  if (_session == nullptr)
    return;

  _session->onInvalidData(data);
}

void ProtocolInterpreter::onCommand(std::string_view command,
                                    std::string_view arguments) {
  size_t commandLength;
  Handler const *handler = findHandler(command, commandLength);
  if (handler == nullptr) {
    std::string commandString(command);
    DS2LOG(Packet, "handler for command '%s' unknown", commandString.c_str());

    //
    // The handler couldn't be found, we don't support this packet.
    //
    _session->sendError(kErrorUnsupported);
    return;
  }

  std::string extra;
  if (commandLength != command.length()) {
    //
    // Command has part of the argument, LLDB doesn't use separators :(
    //
    extra.assign(command.data() + commandLength, command.length() - commandLength);
  }

  extra.append(arguments.data(), arguments.length());

  if (extra.find_first_of("*}") != std::string::npos) {
    extra = Unescape(extra);
    DS2LOG(Packet, "args='%.*s'", static_cast<int>(extra.length()), &extra[0]);
  }

  (handler->handler->*handler->callback)(*handler, extra);
}

bool ProtocolInterpreter::registerHandler(Handler const &handler) {
  if (handler.command.empty() || handler.handler == nullptr ||
      handler.callback == nullptr)
    return false;

  auto it = std::lower_bound(
      _handlers.begin(), _handlers.end(), handler.command,
      [](Handler const &existing, std::string const &command) -> bool {
        return existing.compare(command) < 0;
      });

  if (it != _handlers.end() && it->compare(handler.command) == 0)
    return false;

  _handlers.insert(it, handler);

  return true;
}

ProtocolInterpreter::Handler const *
ProtocolInterpreter::findHandler(std::string_view command,
                                 size_t &commandLength) const {
  auto it = std::lower_bound(
      _handlers.begin(), _handlers.end(), command,
      [](Handler const &handler, std::string_view command) -> bool {
        return handler.compare(command) < 0;
      });

  Handler const *handler = nullptr;
  if (it != _handlers.end() && it->compare(command) == 0) {
    commandLength = it->command.length();
    handler = &(*it);
  }

  return handler;
}

int ProtocolInterpreter::Handler::compare(std::string_view command_) const {
  if (mode == Handler::kModeEquals) {
    return command.compare(command_);
  } else {
    return command.compare(command_.substr(0, command.length()));
  }
}
} // namespace GDBRemote
} // namespace ds2
