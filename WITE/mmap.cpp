/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "mmap.hpp"
#include "DEBUG.hpp"
#include "constants.hpp" //iswindows

#ifdef iswindows

#include <windows.h>
// #include <fileapi.h>
// #include <errhandlingapi.h>

namespace WITE {

  fileHandle openFile(const std::filesystem::path& filename, bool writable, bool clobber) {
    fileHandle ret = CreateFileW(filename.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, clobber ? CREATE_ALWAYS : OPEN_ALWAYS,
				 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_RANDOM_ACCESS, NULL);
    ASSERT_TRAP(ret != INVALID_HANDLE_VALUE, "failed to create/open file ", filename, " with error: ", GetLastError());
    return ret;
  };

  bool lockFile(fileHandle fd) {
    return true;//no-op bc windows files are locked on openFile bc shareMode = 0
  };

  bool unlockFile(fileHandle fd) {
    return true;
  };

  void seekFileEnd(fileHandle fd) {
    //SetFilePointer(fd, 0, NULL, FILE_END); //no-op as long as OVERLAPPED is enabled, offset is provided by writeFile
  };

  bool writeFile(fileHandle fd, const void* src, uint32_t bytes) {
    OVERLAPPED o;
    o.Offset = o.OffsetHigh = 0xFFFFFFFF;
    bool ret = WriteFile(fd, src, bytes, NULL, &o);
    if(!ret) WARN("Writing to file failed, expect problems");
    return ret;
  };

  size_t fileSizeMultiple_impl() {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwAllocationGranularity;
  };

  void* mmapFile(fileHandle fd, size_t start, size_t length) {
    HANDLE mapping = CreateFileMappingA(fd, NULL, PAGE_READWRITE, static_cast<DWORD>(length >> 32), static_cast<DWORD>(length),
					NULL);
    ASSERT_TRAP(mapping, "failed to create file mapping ", GetLastError());
    void* ret = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, static_cast<DWORD>(start >> 32), static_cast<DWORD>(start), length);
    ASSERT_TRAP(ret, "failed to create view map of file");
    return ret;
  };

  void closeMmapFile(void* addr, size_t length) {
    FlushViewOfFile(addr, length);
    UnmapViewOfFile(addr);
    //not checking for success bc calling this for a chunk of ram that's not a view should not be a problem
  };

  void closeFile(fileHandle fd) {
    CloseHandle(fd);
  };

}

#else

#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>

#include "thread.hpp"

namespace WITE {

  fileHandle openFile(const std::filesystem::path& filename, bool writable, bool clobber) {
    fileHandle fd = ::open(filename.c_str(), O_RDWR | O_CREAT | O_NOFOLLOW | O_LARGEFILE | (clobber ? O_TRUNC : 0), 0660);
    ASSERT_TRAP(fd > 0, "failed to open file ", filename, " with errno: ", errno);
    return fd;
  };

  bool lockFile(fileHandle fd) {
    uint32_t sleepCnt = 0;
    int e;
    bool ret;
    do {
      ret = ::flock(fd, LOCK_EX | LOCK_NB) == 0;
      e = errno;
      thread::sleepShort(sleepCnt);//escalating wait time, no wait on first pass
    } while(!ret && e == EAGAIN && sleepCnt < 1000);
    if(!ret) WARN("failed to lock file with errno: ", e);
    return ret;
  };

  bool unlockFile(fileHandle fd) {
    int ret = ::flock(fd, LOCK_UN) == 0;
    bool e = errno;
    if(!ret) WARN("failed to unlock file with errno: ", e);
    return ret;
  };

  void seekFileEnd(fileHandle fd) {
    ::lseek(fd, 0, SEEK_END);
  };

  bool writeFile(fileHandle fd, const void* src, uint32_t bytes) {
    return ::write(fd, src, bytes) == bytes;
  };

  size_t fileSizeMultiple_impl() {
    //almost certainly will always be 4096. Currently that's hard-coded in all x64 linux
    return sysconf(_SC_PAGE_SIZE);
  };

  void* mmapFile(fileHandle fd, size_t start, size_t length) {
    ASSERT_TRAP(length, "attempted to mmap empty region");
    void* ret = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, start);
    #ifdef DEBUG
    auto en = errno;
    #endif
    ASSERT_TRAP(ret && ret != MAP_FAILED, "mmap fail ", en, " fd: ", fd, " start: ", start, " length: ", length);
    return ret;
  };

  void closeMmapFile(void* addr, size_t length) {
    ::msync(addr, length, MS_SYNC);
    //freeing is not needed on unix, the fd closure will handle that
  };

  void closeFile(fileHandle fd) {
    ::close(fd);
  };

}

#endif

namespace WITE {

  size_t fileSizeMultiple_value = 0;

  size_t fileSizeMultiple() {
    if(!fileSizeMultiple_value) [[unlikely]]
      fileSizeMultiple_value = fileSizeMultiple_impl();
    return fileSizeMultiple_value;
  }

}


