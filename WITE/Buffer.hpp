#pragma once

#include "GpuResource.hpp"
#include "PerGpu.hpp"
#include "Vulkan.hpp"
#include "VRam.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  class Gpu;

  class BufferBase : public GpuResource {
  private:
    //TODO views? (requires texel buffer, so probably belongs in a subclass)
    PerGpu<vk::Buffer> buffers;
    PerGpu<vk::DescriptorBufferInfo> accessors;
    PerGpu<std::unique_ptr<VRam>> mem;

    void makeBuffer(vk::Buffer*, size_t gpu);
    static void destroyBuffer(vk::Buffer*, size_t gpu);
  public:
    const BufferSlotData slotData;
    const bool transfersRequired;//if this buffer is to be transferred from it's device: ie if it's used by multiple phys devs
    const size_t size;

    BufferBase(const BufferBase&); //copy con structor only valid before any gpu buffer is allocated (to populate arrays)
    BufferBase(BufferSlotData bsd, GpuResourceInitData grid);
    virtual ~BufferBase() = default;
    vk::Buffer getVkBuffer(size_t gpuIdx);
    void getCreateInfo(Gpu& gpu, vk::BufferCreateInfo* out);
    void populateDSWrite(vk::WriteDescriptorSet* out, size_t gpuIdx) override;
    inline size_t getSize() const { return size; };//TODO dynamic size?
    template<class T> inline void write(const T* data, size_t tCnt, size_t gpu) { mem.getRef(gpu)->write(data, tCnt); };
    template<class T> inline void write(const T* data, size_t tCnt, logicalDeviceMask_t ldm) {
      for(size_t dev : Gpu::gpusForLdm(ldm))
	write(data, tCnt, dev);
    };
  };

  template<BufferSlotData BSD> class Buffer : public BufferBase {
  public:
    Buffer(GpuResourceInitData grid) : BufferBase(BSD, grid) {};
    Buffer(const Buffer<BSD>& b) : BufferBase(b) {};
    ~Buffer() override = default;
    template<class T> inline void write(const T* data, size_t tCnt = 1) { BufferBase::write(data, tCnt, BSD.logicalDeviceMask); };
  };

}

