// Copyright 2022 Saleem Abdulrasool <compnerd@compnerd.org>

#pragma once

#include "DebugServer2/Host/Channel.h"

namespace ds2 {
namespace Host {

class HandleChannel : public Channel {
  int fd_;

public:
  HandleChannel() : fd_(-1) {}
  HandleChannel(int fd);
  ~HandleChannel() override;

  HandleChannel(const HandleChannel &) = delete;
  HandleChannel(HandleChannel &&other) : fd_(other.fd_) {
    other.fd_ = -1;
  }

public:
  void close() override;

public:
  bool connected() const override { return fd_ >= 0; }

public:
  bool wait(int ms = -1) override;

public:
  ssize_t send(void const *buffer, size_t length) override;
  ssize_t receive(void *buffer, size_t length) override;
};

}
}
