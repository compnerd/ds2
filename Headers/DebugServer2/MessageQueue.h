//
// Copyright (c) 2014, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#ifndef __DebugServer2_MessageQueue_h
#define __DebugServer2_MessageQueue_h

#include <DebugServer2/Base.h>

#include <mutex>
#include <condition_variable>
#include <deque>

namespace ds2 {

class MessageQueue {
private:
  std::deque<std::string> _messages;
  std::condition_variable _ready;
  std::mutex _lock;

public:
  MessageQueue();

public:
  void put(std::string const &message);
  std::string get(int wait = -1); // Wait is expressed in milliseconds

  // Wait until the queue is non-empty.  Returns false if
  // the queue is empty after the timeout, true otherwise.
  // (Note that get() may still block after returning if
  // another thread pulls from the queue.)
  bool wait(int ms = -1);

public:
  void clear(bool signal = false);
};
}

#endif // !__DebugServer2_MessageQueue_h
