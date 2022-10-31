#pragma once

#include <atomic>

namespace WITE::Util {

  class FrameCounter {
  private:
    static std::atomic_uint64_t frame;
  public:
    FrameCounter() = delete;
    static void advanceFrame() { frame++; };
    static uint64_t getFrame() { return frame; };
  };

};
