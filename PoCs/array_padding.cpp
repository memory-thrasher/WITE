#include <sys/types.h>
#include <stdint.h>
#include <array>

struct rgb32 {
  float r, g, b;
};

template<size_t L> struct pixelArray {
  std::array<rgb32, L> data;
  static_assert(sizeof(data) == sizeof(uint32_t) * 3 * L);
};

int main(int argc, const char** argv) {
  pixelArray<19> data;
  data.data[0].r = 7;
  return data.data[0].r;
}

