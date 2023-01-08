#pragma once

#include "Vulkan.hpp"

namespace WITE::GPU {

  class Gpu;

  //thin wrapper for vk::DeviceMemory to deallocate when falling out of scope. Plus some metadata, potential for more advanced metrics
  class VRam {
  private:
    vk::DeviceMemory mem;
    VRam(vk::MemoryRequirements mr, uint8_t type, Gpu*);
    friend class Gpu;
  public:
#ifdef DEBUG
    vk::MemoryRequirements mr;//so it can be read by gdb during debugging
#endif
    const vk::MemoryAllocateInfo mai;
    const size_t gpuIdx;
    inline operator vk::DeviceMemory() { return mem; };
    VRam(VRam& o);
    VRam();
    ~VRam();
    void write(const void* src, size_t cnt);
    template<class T> inline void write(const T* data, size_t tCnt) {
      write(reinterpret_cast<const void*>(data), tCnt * sizeof(T));
    };
  };

};

