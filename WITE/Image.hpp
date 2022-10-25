#pragma once

#include "GpuResource.hpp"
#include "Vulkan.hpp"

namespace WITE::GPU {

  class Gpu;

  struct ImageFormatFamily {
    uint8_t components, componentsSize;
    constexpr ImageFormatFamily(uint8_t comp, uint8_t comp_size) : components(comp), componentsSize(comp_size) {};
    friend constexpr auto operator<(const ImageFormatFamily& l, const ImageFormatFamily& r) {
      return l.components < r.components || (l.components == r.components && l.componentsSize < r.componentsSize);
    };
  };

  struct ImageSlotData : public ImageFormatFamily {
    uint8_t dimensions, samples;
    uint32_t arrayLayers, mipLevels;
    uint64_t logicalDeviceMask, usage;
    constexpr ImageSlotData(uint8_t comp, uint8_t comp_size, uint8_t dim, uint8_t sam,
			    uint32_t al, uint32_t mip, uint64_t ldm, uint64_t usage) :
      ImageFormatFamily(comp, comp_size),
      dimensions(dim), samples(sam), arrayLayers(al), mipLevels(mip), logicalDeviceMask(ldm), usage(usage) {};
  };

  class ImageBase : public GpuResource {
  public:

    static constexpr uint32_t
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
      USAGE_IS_CUBE = 0X1000;

    const ImageSlotData slotData;
    const vk::Format format;

    virtual ~ImageBase() = default;
    ImageBase(ImageBase&) = delete;
    ImageBase(ImageSlotData isd);
    void getCreateInfo(Gpu&, void* out, size_t width, size_t height, size_t z = 1);
    vk::ImageUsageFlags getVkUsageFlags();
  };

  //MIP and SAM might be less than requested if the platform does not support it
  template<ImageSlotData ISD> class Image : ImageBase {
  public:
    Image() : ImageBase(ISD) {};
  };

}
