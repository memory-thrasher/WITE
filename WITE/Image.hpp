#pragma once

#include "Vulkan.hpp"

namespace WITE::GPU {

  class Gpu;

  class ImageBase {
  public:

    struct ImageFormatFamily {
      uint8_t components, componentsSize;
      friend constexpr auto operator<(const ImageFormatFamily& l, const ImageFormatFamily& r) {
	return l.components < r.components || (l.components == r.components && l.componentsSize < r.componentsSize);
      };
    };

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

    const uint64_t usage, ldm;
    const uint32_t al, mip;
    const uint8_t dim, comp, comp_size, sam;
    const vk::Format format;

    virtual ~ImageBase() = default;
    ImageBase(ImageBase&) = delete;
    ImageBase(uint64_t usage, uint8_t dim, uint8_t comp, uint8_t comp_size, uint32_t al, uint32_t mip, uint8_t sam, uint64_t ldm);
    void getCreateInfo(Gpu&, void* out, size_t width, size_t height, size_t z = 1);
    vk::ImageUsageFlags getVkUsageFlags();
  };

  //MIP and SAM might be less than requested if the platform does not support it
  template<uint64_t USAGE, uint8_t DIM, uint8_t COMP, uint8_t COMP_SIZE, uint32_t AL, uint32_t MIP, uint8_t SAM, uint64_t LDM>
  class Image : ImageBase {
  public:
    Image() : ImageBase(USAGE, DIM, COMP, COMP_SIZE, AL, MIP, SAM, LDM) {};
  };

}
