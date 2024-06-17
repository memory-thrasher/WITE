/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <iostream>
#include <cstring>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

int main(int argc, char** argv) {
  struct rlimit rlim;
  getrlimit(RLIMIT_DATA, &rlim);
  static constexpr size_t min_size = 1<<10;
  long pageSize = sysconf(_SC_PAGE_SIZE);
  size_t size = ((min_size - 1) / pageSize + 1) * pageSize;
  void*plug = malloc(size);
  memset(plug, 0, size);
  std::cout << "Data rlimit: " << rlim.rlim_cur << std::endl << "page size: " << pageSize << std::endl << "Attempting to map a region of size " << size << " bytes" << std::endl;
  int file = open("mmap_testfile", O_RDWR | O_CREAT | O_NOFOLLOW | O_TRUNC, 0666);
  std::cout << "file opened" << std::endl;
  write(file, plug, size);
  std::cout << "file expanded" << std::endl;
  void* first = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
  std::cout << "first map created with pointer " << first << std::endl;
  memset(first, '0', size);
  std::cout << "first map write finished" << std::endl;
  write(file, plug, size);
  std::cout << "file expanded" << std::endl;
  void* mapped = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, file, size);
  int e = errno;
  if(mapped == NULL) {
    std::cout << "mmap returned NULL and errno is " << e << std::endl;
  } else if(mapped == MAP_FAILED) {
    std::cout << "mmap returned MAP_FAILED and errno is " << e << std::endl;
  } else {
    std::cout << "mmap returned apparently valid point " << mapped << std::endl;
    std::cout << "First bit: " << reinterpret_cast<size_t*>(mapped)[0] << std::endl;
    memset(mapped, '1', size);
    std::cout << "second mapping written" << std::endl;
    msync(mapped, size, MS_SYNC);//msync not strictly necesary but also harmless
    msync(first, size, MS_SYNC);
    munmap(mapped, size);
  }
  free(plug);
}

