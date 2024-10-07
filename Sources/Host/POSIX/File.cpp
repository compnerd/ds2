//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/Host/File.h"
#include "DebugServer2/Host/Platform.h"

#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

#if defined(OS_LINUX)
#include <endian.h>
#elif defined(OS_FREEBSD)
#include <sys/_endian.h>
#elif defined(OS_DARWIN)
#include <libkern/OSByteOrder.h>
#define htobe32(x) OSSwapHostToBigInt32(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#endif

namespace ds2 {
namespace Host {

static int convertFlags(OpenFlags ds2Flags) {
  int flags = 0;

  if (ds2Flags & kOpenFlagRead)
    flags |= (ds2Flags & kOpenFlagWrite) ? O_RDWR : O_RDONLY;
  else if (ds2Flags & kOpenFlagWrite)
    flags |= O_WRONLY;

  if (ds2Flags & kOpenFlagAppend)
    flags |= O_APPEND;
  if (ds2Flags & kOpenFlagTruncate)
    flags |= O_TRUNC;
  if (ds2Flags & kOpenFlagNonBlocking)
    flags |= O_NONBLOCK;
  if (ds2Flags & kOpenFlagCreate)
    flags |= O_CREAT;
  if (ds2Flags & kOpenFlagNewOnly)
    flags |= O_EXCL;
  if (ds2Flags & kOpenFlagNoFollow)
#if defined(O_NOFOLLOW)
    flags |= O_NOFOLLOW;
#else
    return -1;
#endif
  if (ds2Flags & kOpenFlagCloseOnExec)
    flags |= O_CLOEXEC;

  return flags;
}

File::File(std::string const &path, OpenFlags flags, uint32_t mode) {
  int posixFlags = convertFlags(flags);
  if (posixFlags < 0) {
    _lastError = kErrorInvalidArgument;
    return;
  }

  _fd = ::open(path.c_str(), posixFlags, mode);
  _lastError = (_fd < 0) ? Platform::TranslateError() : kSuccess;
}

File::~File() {
  if (valid()) {
    ::close(_fd);
  }
}

ErrorCode File::pread(ByteVector &buf, uint64_t &count, uint64_t offset) {
  if (!valid()) {
    return _lastError = kErrorInvalidHandle;
  }

  if (static_cast<off_t>(offset) > std::numeric_limits<off_t>::max()) {
    return _lastError = kErrorInvalidArgument;
  }

  auto offArg = static_cast<off_t>(offset);
  auto countArg = static_cast<size_t>(count);

  buf.resize(countArg);

  ssize_t nRead = ::pread(_fd, buf.data(), countArg, offArg);

  if (nRead < 0) {
    return _lastError = Platform::TranslateError();
  }

  buf.resize(nRead);
  count = static_cast<uint64_t>(nRead);

  return _lastError = kSuccess;
}

ErrorCode File::pwrite(ByteVector const &buf, uint64_t &count,
                       uint64_t offset) {
  DS2ASSERT(count > 0);

  if (!valid()) {
    return _lastError = kErrorInvalidHandle;
  }

  if (static_cast<off_t>(offset) > std::numeric_limits<off_t>::max()) {
    return _lastError = kErrorInvalidArgument;
  }

  auto offArg = static_cast<off_t>(offset);
  auto countArg = static_cast<size_t>(count);

  ssize_t nWritten = ::pwrite(_fd, buf.data(), countArg, offArg);

  if (nWritten < 0) {
    return _lastError = Platform::TranslateError();
  }

  count = static_cast<uint64_t>(nWritten);

  return _lastError = kSuccess;
}

// lldb expects stat data is returned as a packed buffer with total size of 64
// bytes. The field order is the same as the POSIX defined stat struct. All
// fields are encoded as 4-byte, big-endian unsigned integers except for
// st_size, st_blksize, and st_blocks which are all 8-byte, big-endian unsigned
// integers.
ErrorCode File::fstat(ByteVector &buffer) const {
  struct stat s;
  if (::fstat(_fd, &s) < 0)
    return Platform::TranslateError();

  const auto appendInteger = [&buffer](auto value) -> void {
    buffer.insert(buffer.end(),
                  reinterpret_cast<uint8_t*>(&value),
                  reinterpret_cast<uint8_t*>(&value) + sizeof(value));
  };

  appendInteger(htobe32(static_cast<uint32_t>(s.st_dev)));
  appendInteger(htobe32(static_cast<uint32_t>(s.st_ino)));
  appendInteger(htobe32(static_cast<uint32_t>(s.st_mode)));
  appendInteger(htobe32(static_cast<uint32_t>(s.st_nlink)));
  appendInteger(htobe32(static_cast<uint32_t>(s.st_uid)));
  appendInteger(htobe32(static_cast<uint32_t>(s.st_gid)));
  appendInteger(htobe32(static_cast<uint32_t>(s.st_rdev)));
  appendInteger(htobe64(static_cast<uint64_t>(s.st_size)));
  appendInteger(htobe64(static_cast<uint64_t>(s.st_blksize)));
  appendInteger(htobe64(static_cast<uint64_t>(s.st_blocks)));
  appendInteger(htobe32(static_cast<uint32_t>(s.st_atime)));
  appendInteger(htobe32(static_cast<uint32_t>(s.st_mtime)));
  appendInteger(htobe32(static_cast<uint32_t>(s.st_ctime)));
  DS2ASSERT(buffer.size() == 64);

  return kSuccess;
}

ErrorCode File::chmod(std::string const &path, uint32_t mode) {
  if (::chmod(path.c_str(), mode) < 0) {
    return Platform::TranslateError();
  }

  return kSuccess;
}

ErrorCode File::unlink(std::string const &path) {
  if (::unlink(path.c_str()) < 0) {
    return Platform::TranslateError();
  }

  return kSuccess;
}

ErrorCode File::createDirectory(std::string const &path, uint32_t flags) {
  size_t pos = 0;

  // Each parent directory must be created individually, if it does not exist
  do {
    pos = path.find('/', pos + 1);
    std::string partialPath = path.substr(0, pos);

    if (::mkdir(partialPath.c_str(), flags) < 0) {
      ErrorCode error = Platform::TranslateError();
      if (error == kErrorAlreadyExist) {
        continue;
      }

      return error;
    }
  } while (pos != std::string::npos);

  return kSuccess;
}

ErrorCode File::fileSize(std::string const &path, uint64_t &size) {
  struct stat stbuf;
  if (stat(path.c_str(), &stbuf) < 0)
    return Platform::TranslateError();

  size = static_cast<uint64_t>(stbuf.st_size);
  return kSuccess;
}

ErrorCode File::fileMode(std::string const &path, uint32_t &mode) {
  struct stat stbuf;
  if (stat(path.c_str(), &stbuf) < 0)
    return Platform::TranslateError();

  mode = static_cast<uint32_t>(ALLPERMS & stbuf.st_mode);
  return kSuccess;
}
} // namespace Host
} // namespace ds2
