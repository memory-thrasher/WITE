#pragma once

#include "Vulkan.hpp"
#include "types.hpp"
#include "FrameBufferedCollection.hpp"
#include "Thread.hpp"
#include "PerGpu.hpp"
#include "VRam.hpp"

namespace WITE::GPU {

  struct ImageFormatFamily {
    uint8_t components, componentsSize;
    constexpr ImageFormatFamily(uint8_t comp, uint8_t comp_size) : components(comp), componentsSize(comp_size) {};
    friend constexpr auto operator<(const ImageFormatFamily& l, const ImageFormatFamily& r) {
      return l.components < r.components || (l.components == r.components && l.componentsSize < r.componentsSize);
    };
  };

  struct ImageSlotData : public ImageFormatFamily {
    uint8_t dimensions, samples;
    usage_t usage, externalUsage;//external usage is for render passes to decide what layout to leave it in etc
    logicalDeviceMask_t logicalDeviceMask;
    uint32_t arrayLayers, mipLevels;
    constexpr ImageSlotData(uint8_t comp, uint8_t comp_size, uint8_t dim, uint8_t sam,
			    uint32_t al, uint32_t mip, logicalDeviceMask_t ldm, usage_t usage, usage_t externalUsage) :
      ImageFormatFamily(comp, comp_size),
      dimensions(dim), samples(sam), usage(usage), externalUsage(externalUsage), logicalDeviceMask(ldm), arrayLayers(al), mipLevels(mip) {};
  };

  struct BufferSlotData {
    logicalDeviceMask_t logicalDeviceMask;
    usage_t usage;
    uint32_t size;
  };

  //base class for buffers and images and whatever else might come up
  //all gpu resources shall include a PerGpu and manage any scynchronization between them
  class GpuResource {
  private:
    // static FrameBufferedCollection<GpuResource*> allGpuResources;//TODO garbage collect? Check for leaks and warn? debug only?
    PerGpu<void*> maps;
  protected:
    PerGpuUP<VRam> mem;
  public:
    static constexpr usage_t
    USAGE_INDIRECT = 1,
      USAGE_VERTEX = 2,
      USAGE_DS_READ = 4,
      USAGE_DS_WRITE = 8,
      USAGE_DS_SAMPLED = 0X10,
      USAGE_ATT_DEPTH = 0X20,
      USAGE_ATT_INPUT = 0X40,
      USAGE_ATT_OUTPUT = 0X80,
      USAGE_HOST_READ = 0X100,
      USAGE_HOST_WRITE = 0X200,
      USAGE_TRANSFER = 0x400,
      USAGE_PRESENT = 0x800,
      USAGE_IS_CUBE = 0X1000,
      USAGE_GRAPHICS = 0x2000,//for buffers, implied for images
      USAGE_COMPUTE = 0x4000,//ignored for images, except may affect backing buffer if any
    //meta masks
      MUSAGE_ANY_WRITE = USAGE_DS_WRITE | USAGE_ATT_DEPTH | USAGE_ATT_OUTPUT | USAGE_HOST_WRITE,
      MUSAGE_ANY_READ = USAGE_INDIRECT | USAGE_VERTEX | USAGE_DS_READ | USAGE_DS_SAMPLED | USAGE_ATT_INPUT | USAGE_HOST_READ | USAGE_TRANSFER,
      MUSAGE_ANY_ATTACH = USAGE_ATT_DEPTH | USAGE_ATT_INPUT | USAGE_ATT_OUTPUT,
      MUSAGE_ANY_SHADER_READ = USAGE_INDIRECT | USAGE_VERTEX | USAGE_DS_READ | USAGE_DS_SAMPLED,
      MUSAGE_ANY_HOST = USAGE_HOST_READ | USAGE_HOST_WRITE,
      MUSAGE_ANY_INTERMEDIATE = MUSAGE_ANY_SHADER_READ | USAGE_ATT_INPUT;//denotes which images need synced when multiple gpus
    GpuResource(const GpuResource&) = delete;
    GpuResource() = default;
    virtual ~GpuResource() = default;
    void copy(size_t gpuSrc, size_t gpuDst);
    void* map(size_t gpuIdx);
    virtual void populateDSWrite(vk::WriteDescriptorSet* out, size_t gpuIdx) = 0;//implementation must not overwrite descriptor set, binding or type
  };

  enum class GpuResourceType { eImage, eBuffer };

  struct GpuResourceSlotInfo {
    GpuResourceType type;
    union {
      ImageSlotData imageData;
      BufferSlotData bufferData;
    };
    constexpr GpuResourceSlotInfo(const struct GpuResourceSlotInfo& isd) = default;
    constexpr GpuResourceSlotInfo(ImageSlotData isd) :
      type(GpuResourceType::eImage), imageData(isd) {};
    constexpr GpuResourceSlotInfo(BufferSlotData bsd) :
      type(GpuResourceType::eBuffer), bufferData(bsd) {};
    constexpr GpuResourceSlotInfo mutateUsage(usage_t add, usage_t remove = ~0) const {
      GpuResourceSlotInfo ret = *this;
      switch(type) {
      case GpuResourceType::eImage: ret.imageData.usage = (ret.imageData.usage & ~remove) | add; break;
      case GpuResourceType::eBuffer: ret.bufferData.usage = (ret.bufferData.usage & ~remove) | add; break;
      }
      return ret;
    };
    constexpr inline GpuResourceSlotInfo stagingResourceSlot() const {
      //replace usage with those flags needed to move data in and out for staging
      return mutateUsage(GpuResource::USAGE_TRANSFER | GpuResource::MUSAGE_ANY_HOST);
    };
    constexpr inline GpuResourceSlotInfo externallyStagedResourceSlot() const {
      //remove host access flags, add transfer flag, because transfer will be used to copy to/from staging
      return mutateUsage(GpuResource::USAGE_TRANSFER, GpuResource::MUSAGE_ANY_HOST);
    };
  };

};

