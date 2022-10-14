#include "Image.hpp"

namespace WITE::GPU {

  ImageBase::ImageBase(int64_t usage, int8_t dim, int8_t comp, int8_t comp_size, int32_t al, int32_t mip, int8_t sam, int64_t ldm)
    : format(vk::Format::eUndefined),//format(LogicalGpu.get(ldm).getBestImageFormat(comp, comp_size, usage)),
      usage(usage), ldm(ldm), al(al), mip(mip), dim(dim), comp(comp), comp_size(comp_size), sam(sam)
  {
    // auto components = getComponents();
    // auto size = getComponentsSize();
    //TODO check physical device capabilities, pick the "best" data storage type that supports the usages (likely always unorm)
    //TODO
  }

  void ImageBase::getCreateInfo(Gpu& gpu, void* out, size_t width, size_t height, size_t z) {
#define has(x, r) ((usage & USAGE_ ##x) == USAGE_ ##x ? vk::ImageCreateFlagBits::e ##r : (vk::ImageCreateFlagBits)0)
#define hasu(x, r) ((usage & USAGE_ ##x) == USAGE_ ##x ? vk::ImageUsageFlagBits::e ##r : (vk::ImageUsageFlagBits)0)
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
    				//vv usages
    				hasu(DS_SAMPLED, Sampled) |
    				hasu(DS_WRITE, Storage) |
    				hasu(ATT_OUTPUT, ColorAttachment) |
    				hasu(ATT_DEPTH, DepthStencilAttachment) |
    				hasu(ATT_INPUT, InputAttachment),
    				//^^ usages
    				vk::SharingMode::eExclusive);//default: layout as undefined
  };

}
