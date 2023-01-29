#pragma once

#include <atomic>

#include "Vulkan.hpp"
#include "Callback.hpp"

namespace WITE::GPU {

  class Gpu;

  class Semaphore {
  private:
    std::atomic_uint64_t targetValue;
    static std::atomic_uint64_t idSeed;
    uint64_t id;
    vk::Semaphore sem;
    Gpu* dev;
  public:
    static Util::CallbackPtr<void, Semaphore*, size_t> factory;
    Semaphore() = default;
    Semaphore(Gpu*);
    Semaphore(Semaphore&& o);//move constructor required by PerGpu
    ~Semaphore();
    inline operator vk::Semaphore() { return sem; };
    inline vk::Semaphore vkSem() { return sem; };
    uint64_t notePromise();
    uint64_t getCurrentValue();
    inline uint64_t getPromisedValue() { return targetValue; };
    inline bool pending() { return getCurrentValue() < getPromisedValue(); };
    std::strong_ordering operator<=>(const Semaphore& r) const { return id <=> r.id; };//so semaphore& can be stored in a set
    static void make(Semaphore*, size_t gpu);//for PerGpu
  };

};
