#pragma once

#include <atomic>

namespace WITE::Util {

  class FrameCounter {
  private:
    static std::atomic_uint64_t frame;
  public:
    FrameCounter() = delete;
    static inline void advanceFrame() { frame++; };
    static inline uint64_t getFrame() { return frame; };
  };

};
