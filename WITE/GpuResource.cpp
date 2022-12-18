#include "GpuResource.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  // FrameBufferedCollection<GpuResource*> GpuResource::allGpuResources;//static

  // GpuResource::GpuResource() {
  //   // allGpuResources.push(*this);
  // };

  // GpuResource::~GpuResource() {
  //   // allGpuResources.remove(*this);
  // };

  void* GpuResource::map(size_t gpuIdx) {
    VRam* vram = mem.getRef(gpuIdx).get();
    void** ret = &maps.getRef(gpuIdx);
    if(!*ret)
      VK_ASSERT(Gpu::get(gpuIdx).getVkDevice().mapMemory(*vram, 0, vram->mai.allocationSize, {}, ret), "map failed");
    return *ret;
  };

  void GpuResource::copy(size_t gpuSrc, size_t gpuDst) {
    void* src = map(gpuSrc);
    void* dst = map(gpuDst);
    memcpy(dst, src, mem.getRef(gpuSrc)->mai.allocationSize);//assumes both buffers are the same size
  };

};
