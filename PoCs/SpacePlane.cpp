#include <memory>
#include <iostream>
#include <string.h>
#include <atomic>
#include <pthread.h>

constexpr unsigned long modulusBits = 8, modulusMask = ~((~0ul) << modulusBits), modulus = 1 << modulusBits,
	    pi[3] = { 3, 5, 7 }, pInit[3] = { 13, 17, 19 }, maxDepth = 8, size = 1000;

// constexpr size_t counterCount = 1024*1024;
// std::atomic_uint32_t counters[counterCount];

void* work(void*) {
  static std::atomic_uint32_t xs;
  static std::mutex mutex;
  static std::atomic_bool first = true;
  unsigned int p[3];
  uint32_t x;
  while((x = xs++) < size) {
    for(int y = 0;y < 1;y++) {
      for(int z = 0;z < 1;z++) {
	unsigned long value = 0;
	int lx = x, ly = y, lz = z;
	p[0] = pInit[0];
	p[1] = pInit[1];
	p[2] = pInit[2];
	for(unsigned int b = 0;b < 32;b++) {
	  if(lx & 1) value += p[0];
	  if(ly & 1) value += p[1];
	  if(lz & 1) value += p[2];
	  lx >>= 1;
	  ly >>= 1;
	  lz >>= 1;
	  p[0] *= pi[0];
	  p[1] *= pi[1];
	  p[2] *= pi[2];
	}
	// if((value & modulusMask) == 0) {
	//   if(value / modulus < counterCount)
	//     counters[value / modulus]++;
	// }
	// if(value == modulus * 2076) {
	  mutex.lock();
	  if(!first)
	    std::cout << ", ";
	  else
	    first = false;
	  // std::cout << x << ", " << y << ", " << z << ", 'sk'" << std::flush;
	  std::cout << (value & modulusMask) << std::flush;
	  mutex.unlock();
	// }
      }
    }
  }
  return NULL;
};

int main(int argc, const char** argv) {
  std::cout << "plot3(" << std::flush;
  pthread_t threads[32];
  for(int i = 0;i < 32;i++)
    pthread_create(&threads[i], NULL, &work, NULL);
  for(int i = 0;i < 32;i++)
    pthread_join(threads[i], NULL);
  std::cout << ");\n" << std::flush;
  // for(size_t i = 0;i < counterCount;i++)
  //   if(counters[i])
  //     std::cout << counters[i] << ":: modulus idx " << i << " hit " << counters[i] << " times.\n";
};
