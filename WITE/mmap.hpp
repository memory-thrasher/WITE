/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <filesystem>

//this is an abstraction/polyfill to create a unified mmap interface for windows and unix. unix uses mmap, windows uses FileMapping objects. std::filesystem should be used for non-mmap operations.

namespace WITE {

#ifdef iswindows
  typedef void* fileHandle;//maps to windows HANDLE
#else
  typedef int fileHandle;//fd number on unix
#endif

  //asserts success, if a crash on failure is not desired, potential failure conditions must first be checked using c++ interface
  fileHandle openFile(const std::filesystem::path&, bool writable, bool clobber);

  bool lockFile(fileHandle fd);

  //seekFileEnd + writeFile = append
  //TODO refactor to a single grow command that might also handle remapping IF needed
  void seekFileEnd(fileHandle);

  bool writeFile(fileHandle fd, const void* src, uint32_t bytes);

  void* mmapFile(fileHandle fd, size_t start, size_t length);

  void closeMmapFile(void*, size_t length);

  void closeFile(fileHandle);

  template<class T> bool writeFile(fileHandle fd, const T* data) {
    return WITE::writeFile(fd, reinterpret_cast<const void*>(data), sizeof(T));
  };

  template<class T, size_t CNT> bool writeArrayFile(fileHandle fd, const T data[CNT]) {
    return WITE::writeFile(fd, reinterpret_cast<const void*>(data), CNT*sizeof(T));
  };

  template<class T> bool writeArrayFile(fileHandle fd, const T* data, size_t cnt) {
    return WITE::writeFile(fd, reinterpret_cast<const void*>(data), cnt*sizeof(T));
  };

}
