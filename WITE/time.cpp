#include "time.hpp"

namespace WITE::Util {

  constexpr uint64_t NS_PER_SEC = 1000000000;

  timespec now() {
    timespec ret;
    clock_gettime(CLOCK_MONOTONIC, &ret);
    return ret;
  };

  timespec since(const timespec& b) {
    auto n = now();
    return diff(n, b);
  };

  timespec diff(const timespec& a, const timespec& b) {
    return a.tv_nsec < b.tv_nsec ? {
      a.tv_sec - b.tv_sec - 1,
      NS_PER_SEC + a.tv_nsec - b.tv_nsec
    } : {
      a.tv_sec - b.tv_sec,
      a.tv_nsec - b.tv_nsec
    };
  };

  uint64_t toNS(const timespec& t) {
    return NS_PER_SEC * t.tv_sec + t.tv_nsec;
  };

}
