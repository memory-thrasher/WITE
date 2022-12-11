#include "VRam.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  VRam::VRam(vk::MemoryRequirements mr, uint8_t type, Gpu* dev) :
    mai(mr.size, type),
    gpuIdx(dev->getIndex())
  {
#ifdef DEBUG
    this->mr = mr;
#endif
    VK_ASSERT(dev->getVkDevice().allocateMemory(&mai, ALLOCCB, &mem), "Failed to allocate vram");
  };

  VRam::VRam(VRam& o) : mem(o.mem), mai(o.mai), gpuIdx(o.gpuIdx) {
#ifdef DEBUG
    this->mr = o.mr;
#endif
    o.mem = VK_NULL_HANDLE;
  };

  VRam::VRam() : gpuIdx(0) {};

  VRam::~VRam() {
    if(mem) {
      Gpu& dev = Gpu::get(gpuIdx);
      dev.getVkDevice().freeMemory(mem, ALLOCCB);
      mem = VK_NULL_HANDLE;
      dev.deallocate(this);
    }
  };

};
