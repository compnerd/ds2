// Copyright 2022 Saleem Abdulrasool <compnerd@compnerd.org>

#include "DebugServer2/Host/POSIX/HandleChannel.h"

#include <fcntl.h>
#include <poll.h>

namespace ds2 {
namespace Host {

HandleChannel::HandleChannel(int fd) : fd_(fd) {
  int flags = ::fcntl(fd_, F_GETFL, 0);
  ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
}

HandleChannel::~HandleChannel() {
  close();
}

void HandleChannel::close() {
  if (fd_ >= 0)
    ::close(fd_);
  fd_ = -1;
}

bool HandleChannel::wait(int ms) {
  if (fd_ < 0)
    return false;

  struct pollfd fds;
  fds.fd = fd_;
  fds.events = POLLIN;
  int nfds = ::poll(&fds, 1, ms);
  return nfds == 1 && (fds.revents & POLLIN);
}

ssize_t HandleChannel::send(void const *buffer, size_t length) {
  if (fd_ < 0)
    return -1;

  if (ssize_t nwritten =
          ::write(fd_, reinterpret_cast<const char *>(buffer), length);
      nwritten > 0)
    return nwritten;

  close();
  return -1;
}

ssize_t HandleChannel::receive(void *buffer, size_t length) {
  if (fd_ < 0)
    return -1;

  if (length == 0)
    return 0;

  ssize_t nread = ::read(fd_, reinterpret_cast<char *>(buffer), length);
  if (nread == 0)
    close();
  return nread > 0 ? nread : 0;
}
}
}
