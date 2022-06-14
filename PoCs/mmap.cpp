#include <iostream>
#include <cstring>
#include <sys/mman.h>
#include <sys/resource.h>

int main(int argc, char** argv) {
  void* mapped;
  struct rlimit rlim;
  getrlimit(RLIMIT_DATA, &rlim);
  size_t size = 409600;
  std::cout << "Data rlimit: " << rlim.rlim_cur << std::endl << "Attempting to map a region of size " << size << " bytes" << std::endl;
  mapped = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  int e = errno;
  if(mapped == NULL) {
    std::cout << "mmap returned NULL and errno is " << e << std::endl;
  } else if(mapped == MAP_FAILED) {
    std::cout << "mmap returned MAP_FAILED and errno is " << e << std::endl;
  } else {
    std::cout << "mmap returned apparently valid point " << mapped << std::endl;
    std::cout << "First bit: " << reinterpret_cast<size_t*>(mapped)[0] << std::endl;
    munmap(mapped, size);
  }
}

