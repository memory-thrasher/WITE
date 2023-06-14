#include "VRam.hpp"
#include "Gpu.hpp"
#include "DEBUG.hpp"

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

  void VRam::read(void* dst, size_t cnt) {
    Util::ScopeLock lock(&mmapLock);
    //TODO assert that memory is host visible and/or transfer_dst usable
    auto& gpu = Gpu::get(gpuIdx);
    auto dev = gpu.getVkDevice();
    void* src;
    VK_ASSERT(dev.mapMemory(mem, 0, cnt, {}, &src), "Could not access device memory.");
    //FIXME if mem is not host-visible (see memory requirements), make a temporary buffer first here to stage and dispose after
    //FIXME if map fails, a smaller map might not. Batchify if needed. Hopefully we don't get huge buffers.
    std::memcpy(dst, src, cnt);
    dev.unmapMemory(mem);
  };

  void VRam::write(const void* src, size_t cnt) {
    Util::ScopeLock lock(&mmapLock);
    //TODO assert that memory is host visible and/or transfer_dst usable
    auto& gpu = Gpu::get(gpuIdx);
    auto dev = gpu.getVkDevice();
    void* dst;
    VK_ASSERT(dev.mapMemory(mem, 0, cnt, {}, &dst), "Could not access device memory.");
    //FIXME if mem is not host-visible (see memory requirements), make a temporary buffer first here to stage and dispose after
    //FIXME if map fails, a smaller map might not. Batchify if needed. Hopefully we don't get huge buffers.
    std::memcpy(dst, src, cnt);
    vk::MappedMemoryRange range { mem, 0, cnt };
    VK_ASSERT(dev.flushMappedMemoryRanges(1, &range), "Could not flush mapped memory.");
    dev.unmapMemory(mem);
  };

};
