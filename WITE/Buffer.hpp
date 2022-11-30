#pragma once

#include "GpuResource.hpp"
#include "PerGpu.hpp"
#include "Vulkan.hpp"

namespace WITE::GPU {

  class Gpu;

  class BufferBase : public GpuResource {
  private:
    //TODO views? (requires texel buffer, so probably belongs in a subclass)
    PerGpu<vk::Buffer> buffers;

    vk::Buffer makeBuffer(size_t gpu);
    static void destroyBuffer(vk::Buffer&, size_t gpu);
  public:
    const BufferSlotData slotData;
    const bool transfersRequired;//if this image is to be transferred from it's device: ie if it's used by multiple phys devs

    BufferBase(BufferBase&) = delete;
    BufferBase(BufferSlotData bsd);
    virtual ~BufferBase();
    vk::Buffer getVkBuffer(size_t gpuIdx);
    void getCreateInfo(Gpu& gpu, vk::BufferCreateInfo* out);
  };

  template<BufferSlotData BSD> class Buffer : public BufferBase {
  public:
    Buffer() : BufferBase(BSD) {};
    ~Buffer() override = default;
  };

  template<BufferSlotData BSD> struct GpuResourceFactory<BSD> {
    typedef Buffer<BSD> type;
    auto operator()() { return type(); };
  };

}

