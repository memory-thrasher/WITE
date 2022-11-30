#include "Image.hpp"
#include "Gpu.hpp"
#include "VulkanConverters.hpp"

namespace WITE::GPU {

  ImageBase::ImageBase(const ImageSlotData isd)
    : views(views_t::creator_t_F::make(this, &ImageBase::makeImageView),
	    views_t::destroyer_t_F::make(&ImageBase::destroyImageView)),
      images(PerGpu<vk::Image>::creator_t_F::make(this, &ImageBase::makeImage),
	     PerGpu<vk::Image>::destroyer_t_F::make(&ImageBase::destroyImage)),
      w(512), h(isd.dimensions > 1 ? 512 : 1), z(1),
      slotData(isd),
      format(Gpu::getBestImageFormat(isd.components, isd.componentsSize, isd.usage, isd.logicalDeviceMask)),
      transfersRequired(Gpu::gpuCountByLdm(isd.logicalDeviceMask) > 1)
  {};

  vk::Image ImageBase::makeImage(size_t gpu) {
    vk::Image ret;
    vk::ImageCreateInfo ci;
    Gpu& g = Gpu::get(gpu);
    getCreateInfo(g, &ci, w, h, z);
    VK_ASSERT(g.getVkDevice().createImage(&ci, ALLOCCB, &ret), "Failed to create image");
    return ret;
  };

  void ImageBase::destroyImage(vk::Image& d, size_t gpu) {//static
    Gpu::get(gpu).getVkDevice().destroyImage(d);
  };

  void ImageBase::getCreateInfo(Gpu& gpu, vk::ImageCreateInfo* out, size_t width, size_t height, size_t z) {
    const auto usage = slotData.usage;
    const auto dim = slotData.dimensions;
    const auto sam = slotData.samples;
    const auto al = slotData.arrayLayers;
    const auto mip = slotData.mipLevels;
#define has(x, r) ((usage & USAGE_ ##x) == USAGE_ ##x ? vk::ImageCreateFlagBits::e ##r : (vk::ImageCreateFlagBits)0)
#define hasc(c, r) ((c) ? vk::ImageCreateFlagBits::e ##r : (vk::ImageCreateFlagBits)0)
    *out = vk::ImageCreateInfo(has(IS_CUBE, CubeCompatible) |
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
			       vk::SharingMode::eExclusive //TODO? concurrent access images (on readonly cycle)?
			       );//default: layout as undefined
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

  void ImageBase::makeImageView(vk::ImageView* ret, size_t gpu) {
    vk::ImageViewCreateInfo ivci { {}, getVkImage(gpu), getViewTypeForWhole(slotData), format, {}, {
	aspectMaskForUsage(slotData.usage), 0, slotData.mipLevels, 0, slotData.arrayLayers } };
    VK_ASSERT(Gpu::get(gpu).getVkDevice().createImageView(&ivci, ALLOCCB, ret), "Failed to create image view");
  };

  void ImageBase::destroyImageView(vk::ImageView& doomed, size_t gpu) {//static
    Gpu::get(gpu).getVkDevice().destroyImageView(doomed, ALLOCCB);
  };

  vk::ImageView ImageBase::getVkImageView(size_t gpuIdx) {
    return views[gpuIdx];
  };

}
