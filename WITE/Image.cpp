#include "Image.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  ImageBase::ImageBase(const ImageSlotData isd)
    : slotData(isd),
      format(Gpu::getBestImageFormat(isd.components, isd.componentsSize, isd.usage, isd.logicalDeviceMask))
  {};

  void ImageBase::getCreateInfo(Gpu& gpu, void* out, size_t width, size_t height, size_t z) {
    const auto usage = slotData.usage;
    const auto dim = slotData.dimensions;
    const auto sam = slotData.samples;
    const auto al = slotData.arrayLayers;
    const auto mip = slotData.mipLevels;
#define has(x, r) ((usage & USAGE_ ##x) == USAGE_ ##x ? vk::ImageCreateFlagBits::e ##r : (vk::ImageCreateFlagBits)0)
#define hasc(c, r) ((c) ? vk::ImageCreateFlagBits::e ##r : (vk::ImageCreateFlagBits)0)
    new(out)vk::ImageCreateInfo(has(IS_CUBE, CubeCompatible) |
    				hasc(dim == 2 && al > 1, 2DArrayCompatible) |
    				hasc(sam > 1, SubsampledEXT),
    				//^^flags
    				(vk::ImageType)(dim - 1),//Yes I'm that lazy
    				format,//TODO query for format capabilities
    				vk::Extent3D(width, height, z),
    				mip,//TODO reduce dynamically based on format capabilities
    				al,
    				(vk::SampleCountFlagBits)sam,
    				vk::ImageTiling::eOptimal,
    				getVkUsageFlags(),
    				vk::SharingMode::eExclusive);//default: layout as undefined
#undef has
#undef hasc
  };

  vk::ImageUsageFlags ImageBase::getVkUsageFlags() {
    const auto usage = slotData.usage;
#define hasu(x, r) ((usage & USAGE_ ##x) == USAGE_ ##x ? vk::ImageUsageFlagBits::e ##r : (vk::ImageUsageFlagBits)0)
    return hasu(DS_SAMPLED, Sampled) |
      hasu(DS_WRITE, Storage) |
      hasu(ATT_OUTPUT, ColorAttachment) |
      hasu(ATT_DEPTH, DepthStencilAttachment) |
      hasu(ATT_INPUT, InputAttachment);
#undef hasu
  };

}