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
#include "DebugServer2/Host/Windows/ExtraWrappers.h"
#include "DebugServer2/Utils/String.h"

#include <algorithm>
#include <memory>
#include <windows.h>

namespace ds2 {
namespace Host {

namespace {

DWORD ConvertAccessFlags(OpenFlags flags) {
  DWORD access = 0;
  if (flags & kOpenFlagRead)
    access |= GENERIC_READ;
  if (flags & kOpenFlagWrite)
    access |= GENERIC_WRITE;
  if (flags & kOpenFlagAppend)
    access |= FILE_APPEND_DATA;
  return access;
}

DWORD ConvertCreationDisposition(OpenFlags flags) {
  if (flags & kOpenFlagCreate) {
    if (flags & kOpenFlagNewOnly)
      return CREATE_NEW;
    return (flags & kOpenFlagTruncate) ? CREATE_ALWAYS : OPEN_ALWAYS;
  }
  return (flags & kOpenFlagTruncate) ? TRUNCATE_EXISTING : OPEN_EXISTING;
}

// Windows only models a read-only bit, not the full POSIX permission space;
// synthesize a plausible mode from what actually exists on the file.
uint32_t SynthesizeMode(DWORD attributes) {
  bool isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  bool readOnly = (attributes & FILE_ATTRIBUTE_READONLY) != 0;

  uint32_t mode = isDirectory ? 0040000u : 0100000u; // S_IFDIR / S_IFREG
  if (isDirectory)
    mode |= readOnly ? 0555u : 0755u;
  else
    mode |= readOnly ? 0444u : 0644u;
  return mode;
}

// Windows is always little-endian; reversing the bytes of a little-endian
// value yields its big-endian representation.
template <typename T> T HostToBigEndian(T value) {
  T result;
  const auto *src = reinterpret_cast<const uint8_t *>(&value);
  auto *dst = reinterpret_cast<uint8_t *>(&result);
  for (size_t i = 0; i < sizeof(T); ++i)
    dst[i] = src[sizeof(T) - 1 - i];
  return result;
}

uint64_t FileTimeToUnixTime(const FILETIME &fileTime) {
  ULARGE_INTEGER uli;
  uli.LowPart = fileTime.dwLowDateTime;
  uli.HighPart = fileTime.dwHighDateTime;

  // FILETIME counts 100ns intervals since 1601-01-01; this is the number of
  // such intervals between 1601-01-01 and the Unix epoch (1970-01-01).
  static const uint64_t kEpochDifference = 116444736000000000ULL;
  if (uli.QuadPart < kEpochDifference)
    return 0;

  return (uli.QuadPart - kEpochDifference) / 10000000ULL;
}

// Returns the length of the root component of a normalized (backslash-
// separated) Windows path -- a drive letter ("C:\") or a UNC server/share
// ("\\server\share\") -- or 0 if the path has no recognizable root (e.g. a
// relative path). Hand-rolled rather than using PathCchSkipRoot: pathcch.h's
// functions require Windows 8+, but this project's Windows baseline
// (WINVER/_WIN32_WINNT in CMakeLists.txt) is Vista.
size_t SkipRoot(const std::wstring &path) {
  if (path.size() >= 2 && path[1] == L':') {
    size_t end = 2;
    if (end < path.size() && path[end] == L'\\')
      ++end;
    return end;
  }

  if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\') {
    size_t pos = 2;
    for (int component = 0; component < 2 && pos < path.size(); ++component) {
      size_t separator = path.find(L'\\', pos);
      if (separator == std::wstring::npos) {
        pos = path.size();
        break;
      }
      pos = separator + 1;
    }
    return pos;
  }

  return 0;
}

} // namespace

File::File(const std::string &path, OpenFlags flags, uint32_t mode)
    : _fd(reinterpret_cast<intptr_t>(INVALID_HANDLE_VALUE)),
      _lastError(kErrorInvalidHandle) {
  DWORD access = ConvertAccessFlags(flags);
  DWORD disposition = ConvertCreationDisposition(flags);
  DWORD attributes = ((flags & kOpenFlagCreate) && (mode & 0222) == 0)
                         ? FILE_ATTRIBUTE_READONLY
                         : FILE_ATTRIBUTE_NORMAL;
  // Mirror O_NOFOLLOW: open the reparse point itself instead of
  // transparently traversing a symlink/junction to its target.
  if (flags & kOpenFlagNoFollow)
    attributes |= FILE_FLAG_OPEN_REPARSE_POINT;

  HANDLE handle =
      ::CreateFileW(ds2::Utils::NarrowToWideString(path).c_str(), access,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, disposition, attributes, nullptr);

  if (handle == INVALID_HANDLE_VALUE) {
    _lastError = Platform::TranslateError();
    return;
  }

  if (flags & kOpenFlagNoFollow) {
    BY_HANDLE_FILE_INFORMATION info;
    if (::GetFileInformationByHandle(handle, &info) &&
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
      // The path names a symlink/junction; fail instead of handing back a
      // reparse-point handle to it, just as the POSIX O_NOFOLLOW path fails
      // to open a symlink rather than silently following it.
      ::CloseHandle(handle);
      _lastError = kErrorInvalidArgument;
      return;
    }
  }

  _fd = reinterpret_cast<intptr_t>(handle);
  _lastError = kSuccess;
}

File::~File() {
  if (valid())
    ::CloseHandle(reinterpret_cast<HANDLE>(_fd));
}

ErrorCode File::pread(ByteVector &buf, uint64_t &count, uint64_t offset) {
  if (!valid())
    return _lastError = kErrorInvalidHandle;

  auto countArg = static_cast<DWORD>(count);
  buf.resize(countArg);

  OVERLAPPED overlapped = {};
  overlapped.Offset = static_cast<DWORD>(offset & 0xffffffffull);
  overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);

  DWORD nRead = 0;
  if (!::ReadFile(reinterpret_cast<HANDLE>(_fd), buf.data(), countArg, &nRead,
                  &overlapped)) {
    // A positioned read (via the OVERLAPPED offset fields) at or past the
    // end of the file fails with ERROR_HANDLE_EOF on a synchronous handle,
    // unlike an unpositioned read, which just returns 0 bytes. Treat it as
    // a normal, successful end-of-file read rather than an error.
    DWORD error = ::GetLastError();
    if (error != ERROR_HANDLE_EOF)
      return _lastError = Platform::TranslateError(error);
    nRead = 0;
  }

  buf.resize(nRead);
  count = static_cast<uint64_t>(nRead);

  return _lastError = kSuccess;
}

ErrorCode File::pwrite(const ByteVector &buf, uint64_t &count,
                       uint64_t offset) {
  DS2ASSERT(count > 0);

  if (!valid())
    return _lastError = kErrorInvalidHandle;

  auto countArg = static_cast<DWORD>(count);

  OVERLAPPED overlapped = {};
  overlapped.Offset = static_cast<DWORD>(offset & 0xffffffffull);
  overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);

  DWORD nWritten = 0;
  if (!::WriteFile(reinterpret_cast<HANDLE>(_fd), buf.data(), countArg,
                   &nWritten, &overlapped))
    return _lastError = Platform::TranslateError();

  count = static_cast<uint64_t>(nWritten);

  return _lastError = kSuccess;
}

// lldb expects stat data is returned as a packed buffer with total size of 64
// bytes. The field order is the same as the POSIX defined stat struct. All
// fields are encoded as 4-byte, big-endian unsigned integers except for
// st_size, st_blksize, and st_blocks which are all 8-byte, big-endian unsigned
// integers.
ErrorCode File::fstat(ByteVector &buffer) const {
  if (!valid())
    return kErrorInvalidHandle;

  BY_HANDLE_FILE_INFORMATION info;
  if (!::GetFileInformationByHandle(reinterpret_cast<HANDLE>(_fd), &info))
    return Platform::TranslateError();

  uint64_t size =
      (static_cast<uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
  uint32_t mode = SynthesizeMode(info.dwFileAttributes);
  uint64_t blockSize = static_cast<uint64_t>(Platform::GetPageSize());
  uint64_t blocks = blockSize == 0 ? 0 : (size + blockSize - 1) / blockSize;

  const auto appendInteger = [&buffer](auto value) -> void {
    auto be = HostToBigEndian(value);
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t *>(&be),
                  reinterpret_cast<uint8_t *>(&be) + sizeof(be));
  };

  appendInteger(uint32_t(0));                                // st_dev
  appendInteger(uint32_t(0));                                // st_ino
  appendInteger(mode);                                       // st_mode
  appendInteger(static_cast<uint32_t>(info.nNumberOfLinks)); // st_nlink
  appendInteger(uint32_t(0));                                // st_uid
  appendInteger(uint32_t(0));                                // st_gid
  appendInteger(uint32_t(0));                                // st_rdev
  appendInteger(size);                                       // st_size
  appendInteger(blockSize);                                  // st_blksize
  appendInteger(blocks);                                     // st_blocks
  appendInteger(static_cast<uint32_t>(
      FileTimeToUnixTime(info.ftLastAccessTime))); // st_atime
  appendInteger(static_cast<uint32_t>(
      FileTimeToUnixTime(info.ftLastWriteTime))); // st_mtime
  appendInteger(static_cast<uint32_t>(
      FileTimeToUnixTime(info.ftCreationTime))); // st_ctime
  DS2ASSERT(buffer.size() == 64);

  return kSuccess;
}

ErrorCode File::chmod(const std::string &path, uint32_t mode) {
  std::wstring widePath = ds2::Utils::NarrowToWideString(path);

  DWORD attributes = ::GetFileAttributesW(widePath.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES)
    return Platform::TranslateError();

  // Windows only models a read-only bit; map the owner-write permission bit
  // onto it since that is the closest POSIX equivalent.
  if (mode & 0200)
    attributes &= ~FILE_ATTRIBUTE_READONLY;
  else
    attributes |= FILE_ATTRIBUTE_READONLY;

  if (!::SetFileAttributesW(widePath.c_str(), attributes))
    return Platform::TranslateError();

  return kSuccess;
}

ErrorCode File::unlink(const std::string &path) {
  if (!::DeleteFileW(ds2::Utils::NarrowToWideString(path).c_str()))
    return Platform::TranslateError();

  return kSuccess;
}

ErrorCode File::createDirectory(const std::string &path, uint32_t flags) {
  // Normalize '/' to '\' first: paths arrive from the wire and may use
  // either separator, but the component-splitting loop below only
  // recognizes '\' as a boundary.
  std::wstring canonicalStr = ds2::Utils::NarrowToWideString(path);
  std::replace(canonicalStr.begin(), canonicalStr.end(), L'/', L'\\');

  // Skip the root -- a drive letter ("C:\") or a UNC server/share
  // ("\\server\share\") -- neither of which can themselves be created as a
  // directory. SkipRoot returns 0 for a path with no recognizable root (a
  // relative path); walk the whole string in that case instead of treating
  // it as an error, matching how a relative path was always handled here.
  size_t searchFrom = SkipRoot(canonicalStr);

  // Each parent directory must be created individually, if it does not
  // exist.
  for (;;) {
    size_t boundary = canonicalStr.find(L'\\', searchFrom);
    std::wstring partialPath = canonicalStr.substr(0, boundary);
    if (!partialPath.empty() &&
        !::CreateDirectoryW(partialPath.c_str(), nullptr)) {
      ErrorCode error = Platform::TranslateError();
      if (error != kErrorAlreadyExist)
        return error;
    }

    if (boundary == std::wstring::npos)
      return kSuccess;
    searchFrom = boundary + 1;
  }
}

ErrorCode File::fileSize(const std::string &path, uint64_t &size) {
  WIN32_FILE_ATTRIBUTE_DATA attributes;
  if (!::GetFileAttributesExW(ds2::Utils::NarrowToWideString(path).c_str(),
                              GetFileExInfoStandard, &attributes))
    return Platform::TranslateError();

  size = (static_cast<uint64_t>(attributes.nFileSizeHigh) << 32) |
         attributes.nFileSizeLow;
  return kSuccess;
}

ErrorCode File::fileMode(const std::string &path, uint32_t &mode) {
  WIN32_FILE_ATTRIBUTE_DATA attributes;
  if (!::GetFileAttributesExW(ds2::Utils::NarrowToWideString(path).c_str(),
                              GetFileExInfoStandard, &attributes))
    return Platform::TranslateError();

  // Match the POSIX implementation (ALLPERMS & st_mode): vFile:mode reports
  // permission bits only. SynthesizeMode() also encodes the S_IFDIR/S_IFREG
  // file-type bits fstat's st_mode needs, so mask those back out here.
  mode = SynthesizeMode(attributes.dwFileAttributes) & 0777;
  return kSuccess;
}

} // namespace Host
} // namespace ds2
