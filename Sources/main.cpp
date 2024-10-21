//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Core/BreakpointManager.h"
#include "DebugServer2/Core/SessionThread.h"
#include "DebugServer2/GDBRemote/DebugSessionImpl.h"
#include "DebugServer2/GDBRemote/PlatformSessionImpl.h"
#include "DebugServer2/GDBRemote/ProtocolHelpers.h"
#include "DebugServer2/Host/Platform.h"
#include "DebugServer2/Host/QueueChannel.h"
#include "DebugServer2/Host/Socket.h"
#if defined(OS_POSIX)
#include "DebugServer2/Host/POSIX/HandleChannel.h"
#endif
#include "DebugServer2/Utils/Daemon.h"
#include "DebugServer2/Utils/Log.h"
#include "DebugServer2/Utils/OptParse.h"
#include "DebugServer2/Utils/String.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <set>
#include <string>
#include <thread>

#include <fcntl.h>
#if HAVE_TERMIOS_H
#include <termios.h>
#endif
#if !defined(OS_WIN32)
#include <unistd.h>
#endif

using ds2::GDBRemote::DebugSessionImpl;
using ds2::GDBRemote::PlatformSessionImpl;
using ds2::GDBRemote::Session;
using ds2::GDBRemote::SessionDelegate;
using ds2::Host::Platform;
using ds2::Host::QueueChannel;
using ds2::Host::Socket;

static std::string gDefaultPort = "12345";
static std::string gDefaultHost = "127.0.0.1";
static bool gDaemonize = false;
static bool gGDBCompat = false;

#if defined(OS_POSIX)
static void CloseFD() {
  for (int i = 3; i < 1024; ++i)
    ::close(i);
}

static void CloseFD(int fd) {
  for (int i = 3; i < fd; ++i)
    ::close(i);
  for (int i = fd + 1; i < 1024; ++i)
    ::close(i);
}
#endif

#if defined(OS_POSIX)
static std::unique_ptr<Socket> CreateFDSocket(int fd) {
  errno = 0;
  int flags = ::fcntl(fd, F_GETFL);
  if (flags == -1) {
    DS2LOG(Error, "cannot use fd %d: %s", fd, strerror(errno));
  }

  if (::isatty(fd)) {
    DS2LOG(Error, "cannot use fd %d: refers to a terminal", fd);
  }

  auto socket = ds2::make_unique<Socket>(fd);

  return socket;
}
#endif

// This function creates and initializes a socket than can be either a client
// (if reverse is true), or a server (if reverse is false) that will then need
// to be accept()'d on.
static std::unique_ptr<Socket> CreateTCPSocket(std::string const &host,
                                               std::string const &port,
                                               bool reverse) {
  auto socket = ds2::make_unique<Socket>();

  if (reverse) {
    if (!socket->connect(host, port)) {
      DS2LOG(Fatal, "cannot connect to [%s:%s]: %s", host.c_str(), port.c_str(),
             socket->error().c_str());
    } else {
      DS2LOG(Debug, "connected to [%s:%s]", socket->address().c_str(),
             socket->port().c_str());
    }
  } else {
    if (!socket->listen(host, port)) {
      DS2LOG(Fatal, "cannot listen on [%s:%s]: %s", host.c_str(), port.c_str(),
             socket->error().c_str());
    } else {
      // This print to stderr is required when gdb launches ds2
      if (gGDBCompat) {
        ::fprintf(stderr, "Listening on port %s\n", socket->port().c_str());
      }

      DS2LOG(Debug, "listening on [%s:%s]", socket->address().c_str(),
             socket->port().c_str());
    }
  }

  return socket;
}

#if defined(OS_POSIX)
static std::unique_ptr<Socket> CreateUNIXSocket(std::string const &path,
                                                bool abstract, bool reverse) {
#if !defined(OS_LINUX)
  if (abstract) {
    DS2LOG(Fatal, "abstract UNIX sockets not supported on this system");
  }
#endif

  if (reverse) {
    DS2LOG(Fatal, "reverse connections not supported with UNIX sockets");
  }

  auto socket = ds2::make_unique<Socket>();

  if (!socket->listen(path, abstract)) {
    DS2LOG(Fatal, "cannot listen on %s: %s", path.c_str(),
           socket->error().c_str());
  } else {
    DS2LOG(Debug, "listening on %s", path.c_str());
  }

  return socket;
}
#endif

static std::unique_ptr<Socket> CreateSocket(std::string const &arg,
                                            bool reverse) {
  std::string protocol;
  std::string addrString;

  auto protocolSplitPos = arg.find("://");
  if (protocolSplitPos == std::string::npos) {
    protocol = "tcp";
    addrString = arg;
  } else {
    protocol = arg.substr(0, protocolSplitPos);
    addrString = arg.substr(protocolSplitPos + ::strlen("://"));
  }

  if (protocol == "tcp") {
    std::string host, port;

    auto splitPos = addrString.rfind(":");

    // In certain cases, the [host]:port specification is just an integer
    // representing the port, without the separating colon. If that happens,
    // just use the default host to create the socket.
    if (splitPos == std::string::npos) {
      port = addrString;
    } else {
      if (splitPos > 0) {
        // IPv6 addresses can be of the form '[a:b:c:d::1]:12345', so we need
        // to strip the square brackets around the host part.
        if (addrString[0] == '[' && addrString[splitPos - 1] == ']') {
          host = addrString.substr(1, splitPos - 2);
        } else if (splitPos == 1 && addrString[0] == '*') {
          // Allow caller to specify a host name of * to listen on all available
          // network interfaces. On some, but not all platforms, getaddrinfo()
          // resolves * to 0.0.0.0 already. Explicitly making this substitution
          // ensures the most compatibility.
          host = "0.0.0.0";
        } else {
          host = addrString.substr(0, splitPos);
        }
      }

      port = addrString.substr(splitPos + 1);
    }

    return CreateTCPSocket(host.empty() ? gDefaultHost : host,
                           port.empty() ? gDefaultPort : port, reverse);
#if defined(OS_POSIX)
  } else if (protocol == "unix" || protocol == "unix-abstract") {
    return CreateUNIXSocket(addrString, protocol == "unix-abstract", reverse);
#endif
  } else {
    DS2LOG(Fatal, "unknown protocol `%s'", protocol.c_str());
  }

  DS2_UNREACHABLE();
}

static int RunDebugServer(ds2::Host::Channel *channel, SessionDelegate *impl) {
  Session session(gGDBCompat ? ds2::GDBRemote::kCompatibilityModeGDB
                             : ds2::GDBRemote::kCompatibilityModeLLDB);
  QueueChannel qchannel(channel);
  SessionThread thread(&qchannel, &session);

  session.setDelegate(impl);
  session.create(&qchannel);

  DS2LOG(Debug, "Debug session starting");
  thread.start();
  while (session.receive(/*cooked=*/true))
    continue;
  DS2LOG(Debug, "Debug session ended");

  return EXIT_SUCCESS;
}

static void AddSharedOptions(ds2::OptParse &opts) {
  opts.addOption(ds2::OptParse::stringOption, "log-file", 'o',
                 "output log messages to the file specified");
  opts.addOption(ds2::OptParse::boolOption, "remote-debug", 'D',
                 "enable log for remote protocol packets");
  opts.addOption(ds2::OptParse::boolOption, "debug", 'd',
                 "enable debug log output");
  opts.addOption(ds2::OptParse::boolOption, "no-colors", 'n',
                 "disable colored output");

#if defined(OS_POSIX)
  opts.addOption(ds2::OptParse::boolOption, "daemonize", 'f',
                 "detach and become a daemon");
  opts.addOption(ds2::OptParse::boolOption, "setsid", 'S',
                 "make ds2 run in its own session");
#endif
}

static void HandleSharedOptions(ds2::OptParse const &opts) {
  if (!opts.getString("log-file").empty()) {
    ds2::SetLogOutputFilename(opts.getString("log-file").c_str());
    ds2::SetLogColorsEnabled(false);
    ds2::SetLogLevel(ds2::kLogLevelDebug);
  }

  if (opts.getBool("remote-debug")) {
    ds2::SetLogLevel(ds2::kLogLevelPacket);
  } else if (opts.getBool("debug")) {
    ds2::SetLogLevel(ds2::kLogLevelDebug);
  }

  if (opts.getBool("no-colors")) {
    ds2::SetLogColorsEnabled(false);
  }

#if defined(OS_POSIX)
  gDaemonize = opts.getBool("daemonize");

  if (opts.getBool("setsid")) {
    ::setsid();
  }
#endif
}

static int GdbserverMain(int argc, char **argv) {
  DS2ASSERT(argv[1][0] == 'g');

  ds2::OptParse opts;
  AddSharedOptions(opts);

  // Target debug options.
  opts.addOption(ds2::OptParse::vectorOption, "set-env", 'e',
                 "add an element to the environment before launch");
  opts.addOption(ds2::OptParse::vectorOption, "unset-env", 'E',
                 "remove an element from the environment before lauch");
  opts.addOption(ds2::OptParse::stringOption, "attach", 'a',
                 "attach to the name or PID specified");

  // lldb-server compatibility options.
  opts.addOption(ds2::OptParse::boolOption, "gdb-compat", 'g',
                 "force ds2 to run in gdb compat mode");
  opts.addOption(ds2::OptParse::stringOption, "named-pipe", 'N',
                 "determine a port dynamically and write back to FIFO");
  opts.addOption(ds2::OptParse::boolOption, "reverse-connect", 'R',
                 "connect back to the debugger at [HOST]:PORT");
  opts.addOption(ds2::OptParse::boolOption, "native-regs", 'r',
                 "use native registers (no-op)", true);

#if defined(OS_POSIX)
  opts.addOption(ds2::OptParse::stringOption, "fd", 'F',
                 "use a file descriptor to communicate");
#endif

  // gdbserver compatibility options.
  opts.addOption(ds2::OptParse::boolOption, "once", 'O',
                 "exit after one execution of inferior (default)", true);

  // [host]:port positional argument.
  opts.addPositional("[host]:port", "the [host]:port to connect to");

  int idx = opts.parse(argc, argv);

  enum class channel_type {
    file_descriptor,
    character_device,
    named_pipe,
    network,
  };
  channel_type connection_type = channel_type::network;
  int fd = -1;

#if defined(OS_POSIX)
  if (const std::string &arg = opts.getString("fd"); !arg.empty()) {
    connection_type = channel_type::file_descriptor;
    fd = atoi(arg.c_str());
    if (fd < 0) {
      DS2LOG(Error, "invalid file descriptor %s", arg.c_str());
      connection_type = channel_type::network;
    }
  }
#endif

  // This is used for llgs testing. We determine a port number dynamically and
  // write it back to the FIFO passed as argument for the test harness to use
  // it.
  const std::string &pipe = opts.getString("named-pipe");
  if (!pipe.empty())
    connection_type = channel_type::named_pipe;

  const std::string &address = opts.getPositional("[host]:port");
#if defined(OS_POSIX)
  if (!address.empty() && address.find_first_of(':') == std::string::npos)
    connection_type = channel_type::character_device;
#endif

  switch (connection_type) {
  case channel_type::file_descriptor:
#if defined(OS_POSIX)
    CloseFD(fd);
    break;
#else
    (void)fd;
    DS2BUG("connecting with file descriptor is not supported on this platform");
#endif
  default:
#if defined(OS_POSIX)
    CloseFD();
#endif
    break;
  }

  HandleSharedOptions(opts);

  // Target debug program and arguments.
  ds2::StringCollection args(&argv[idx], &argv[argc]);

  // Target debug environment.
  ds2::EnvironmentBlock env;
  Platform::GetCurrentEnvironment(env);

  for (auto const &e : opts.getVector("set-env")) {
    char const *arg = e.c_str();
    char const *equal = strchr(arg, '=');
    if (equal == nullptr || equal == arg) {
      DS2LOG(Error, "trying to add invalid environment value '%s', skipping",
             e.c_str());
      continue;
    }
    env[std::string(arg, equal)] = equal + 1;
  }

  for (auto const &e : opts.getVector("unset-env")) {
    if (env.find(e) == env.end()) {
      DS2LOG(Warning, "trying to remove '%s' not present in the environment",
             e.c_str());
      continue;
    }
    env.erase(e);
  }

  int attachPid = opts.getString("attach").empty()
                      ? -1
                      : atoi(opts.getString("attach").c_str());

  gGDBCompat = opts.getBool("gdb-compat");
  if (gGDBCompat && args.empty() && attachPid < 0) {
    // In GDB compatibility mode, we need a process to attach to or a command
    // line so we can launch it.
    // In LLDB mode, we can launch the debug server without any of those two
    // things, and wait for an "A" command that specifies the command line to
    // use to launch the inferior.
    opts.usageDie("either a program or target PID is required in gdb mode");
  }

  bool reverse = opts.getBool("reverse-connect");

  std::unique_ptr<Socket> socket;
#if defined(OS_POSIX)
  std::unique_ptr<ds2::Host::HandleChannel> device;
#endif

  switch (connection_type) {
  case channel_type::named_pipe:
#if defined(OS_POSIX)
    if (fd >= 0)
      DS2LOG(Error, "named pipe should not be used with fd option");
#endif
    [[fallthrough]];
  case channel_type::network:
    // If we have a named pipe, set the port to 0 to indicate that we should
    // dynamically allocate it and write it back to the FIFO.
    socket = address.empty()
                 ? CreateTCPSocket(gDefaultHost,
                                   pipe.empty() ? gDefaultPort : "0", reverse)
                 : CreateSocket(address, reverse);
    if (connection_type == channel_type::named_pipe) {
      std::string port = socket->port();
      FILE *named_pipe = ::fopen(pipe.c_str(), "a");
      if (named_pipe) {
        ::fwrite(port.c_str(), 1, port.length() + 1, named_pipe);
        ::fclose(named_pipe);
      } else {
        DS2LOG(Error, "unable to open %s: %s", pipe.c_str(), strerror(errno));
      }
    }
    break;

  case channel_type::file_descriptor:
#if defined(OS_POSIX)
    socket = CreateFDSocket(fd);
#else
    (void)fd;
    DS2BUG("connecting with file descriptor is not supported on this platform");
#endif
    break;

  case channel_type::character_device:
#if defined(OS_POSIX)
    if (std::filesystem::exists(address))
      if (std::filesystem::is_character_file(address) ||
          std::filesystem::is_fifo(address))
        fd = ::open(address.c_str(), O_RDWR);
    if (fd < 0) {
      DS2LOG(Error, "unable to open %s: %s", address.c_str(), strerror(errno));
      ::exit(EXIT_FAILURE);
    }

#if HAVE_TERMIOS_H
    {
      struct termios termios;
      (void)tcgetattr(fd, &termios);
      termios.c_iflag = 0;
      termios.c_oflag = 0;
      termios.c_cflag = (termios.c_cflag & ~(CSIZE | PARENB)) | CLOCAL | CS8;
      termios.c_lflag = 0;
      termios.c_cc[VMIN] = 1;
      termios.c_cc[VTIME] = 0;
      (void)tcsetattr(fd, TCSANOW, &termios);
    }
#endif

    device = std::make_unique<ds2::Host::HandleChannel>(fd);
    break;
#else
    DS2BUG("connecting with chardev is not supported on this platform");
#endif
  }

  if (gDaemonize) {
    ds2::Utils::Daemonize();
  }

  std::unique_ptr<ds2::Host::Channel> channel;
  switch (connection_type) {
  case channel_type::file_descriptor:
  case channel_type::named_pipe:
  case channel_type::network:
    if (fd >= 0 || reverse) {
      channel.reset(socket.release());
    } else {
      channel = socket->accept();
    }
    break;
  case channel_type::character_device:
#if defined(OS_POSIX)
    channel.reset(device.release());
#else
    DS2BUG("connecting with chardev is not supported on this platform");
#endif
    break;
  }

  std::unique_ptr<DebugSessionImpl> impl;

  if (attachPid > 0)
    impl = ds2::make_unique<DebugSessionImpl>(attachPid);
  else if (args.size() > 0)
    impl = ds2::make_unique<DebugSessionImpl>(args, env);
  else
    impl = ds2::make_unique<DebugSessionImpl>();

  return RunDebugServer(channel.get(), impl.get());
}

#if !defined(OS_WIN32)
static int PlatformMain(int argc, char **argv) {
  DS2ASSERT(argv[1][0] == 'p');

  ds2::OptParse opts;
  AddSharedOptions(opts);

  opts.addOption(ds2::OptParse::stringOption, "listen", 'l',
                 "specify the [host]:port to listen on");
  opts.addOption(ds2::OptParse::boolOption, "server", 's',
                 "create a new process for each client (default)", true);

  opts.parse(argc, argv);
  HandleSharedOptions(opts);

  if (opts.getString("listen").empty()) {
    opts.usageDie("--listen required in platform mode");
  }

  struct PlatformClient {
    std::unique_ptr<Socket> socket;
    PlatformSessionImpl impl;
    Session session;

    PlatformClient(std::unique_ptr<Socket> socket_)
        : socket(std::move(socket_)),
          session(ds2::GDBRemote::kCompatibilityModeLLDB) {
      session.setDelegate(&impl);
      session.create(socket.get());
    }
  };

  std::unique_ptr<Socket> serverSocket =
      CreateSocket(opts.getString("listen"), false);

  if (gDaemonize) {
    ds2::Utils::Daemonize();
  }

  do {
    std::unique_ptr<Socket> clientSocket = serverSocket->accept();
    auto platformClient =
        ds2::make_unique<PlatformClient>(std::move(clientSocket));

    std::thread thread(
        [](std::unique_ptr<PlatformClient> client) {
          while (client->session.receive(/*cooked=*/false)) {
            continue;
          }
        },
        std::move(platformClient));

    thread.detach();
  } while (true);
}

static int ServerMain(int argc, char **argv) {
  DS2ASSERT(argv[1][0] == 's');

  ds2::OptParse opts;
  AddSharedOptions(opts);
  opts.parse(argc, argv);
  HandleSharedOptions(opts);

  std::unique_ptr<Socket> server = CreateTCPSocket(gDefaultHost, "0", false);
  std::string port = server->port();

  pid_t pid = ::fork();
  if (pid < 0) {
    DS2LOG(Fatal, "cannot fork: %s", strerror(errno));
  }

  if (pid == 0) {
    // When in server mode, output is suppressed but for standard error.
    close(0);
    close(1);

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);

    std::unique_ptr<Socket> client = server->accept();

    DebugSessionImpl impl;
    return RunDebugServer(client.get(), &impl);
  } else {
    // Write to the standard output to let our parent know
    // where we're listening.
    ::fprintf(stdout, "%s %d\n", port.c_str(), pid);
  }

  return EXIT_SUCCESS;
}
#endif

static int VersionMain(int argc, char **argv) {
  std::stringstream ss;
  ss << "ds2 at ";
  if (::strlen(DS2_GIT_HASH) != 0) {
    ss << "revision " << DS2_GIT_HASH;
  } else {
    ss << "unknown revision";
  }
  ss << std::endl;

  ::fprintf(stdout, "%s", ss.str().c_str());
  return EXIT_SUCCESS;
}

[[noreturn]] static void UsageDie(char const *argv0) {
  std::vector<std::pair<std::string, bool>> modes = {{"version", false},
                                                     {"gdbserver", true}};
#if !defined(OS_WIN32)
  modes.emplace_back("platform", true);
#endif

  ::fprintf(stderr, "Usage:\n");
  for (auto const &mode : modes) {
    auto c_str = mode.first.c_str();
    ::fprintf(stderr, "  %s [%c]%s%s\n", argv0, c_str[0], c_str + 1,
              mode.second ? " [options]" : "");
  }
  ::exit(EXIT_FAILURE);
}

#if defined(OS_WIN32)
// On certain Windows targets (Windows Phone in particular), we build ds2 as a
// library and load it from another process. To achieve this, we need `main` to
// be exported so we can `GetProcAddress` it from that separate process.
// clang-format off
__declspec(dllexport)
#endif
int main(int argc, char **argv) {
  // clang-format on

  ds2::Host::Platform::Initialize();
#if !defined(OS_WIN32)
  ds2::SetLogColorsEnabled(isatty(fileno(stderr)));
#endif
  ds2::SetLogLevel(ds2::kLogLevelInfo);

  if (argc < 2) {
    UsageDie(argv[0]);
  }

  if (argv[1][0] == 'g')
    return GdbserverMain(argc, argv);

#if defined(OS_POSIX)
  // When ds2 is launched inside the lldb test-runner, python will leak file
  // descriptors to its inferior (ds2). Clean up these file descriptors before
  // running. postpone this into GdbserverMain because --fd may be used
  CloseFD();
#endif

  switch (argv[1][0]) {
#if !defined(OS_WIN32)
  case 'p':
    return PlatformMain(argc, argv);
  case 's':
    return ServerMain(argc, argv);
#endif
  case 'v':
    return VersionMain(argc, argv);
  default:
    UsageDie(argv[0]);
  }
}
