#include "Image.hpp"
#include "Gpu.hpp"
#include "VulkanConverters.hpp"

namespace WITE::GPU {

  ImageBase::ImageBase(const ImageSlotData isd)
    : accessors(accessors_t::creator_t_F::make(this, &ImageBase::makeAccessors),
		accessors_t::destroyer_t_F::make(&ImageBase::destroyAccessors)),
      images(PerGpu<vk::Image>::creator_t_F::make(this, &ImageBase::makeImage),
	     PerGpu<vk::Image>::destroyer_t_F::make(&ImageBase::destroyImage)),
      w(512), h(isd.dimensions > 1 ? 512 : 1), z(1),
      accessStateTracker(PerGpu<StateSynchronizer<accessState>>::creator_t_F::make(this, &ImageBase::makeStateTracker)),
      slotData(isd),
      format(getBestImageFormat(isd)),
      transfersRequired(Gpu::gpuCountByLdm(isd.logicalDeviceMask) > 1 && (isd.externalUsage & MUSAGE_ANY_INTERMEDIATE))
  {};

  void ImageBase::makeImage(vk::Image* ret, size_t gpu) {
    vk::ImageCreateInfo ci;
    Gpu& g = Gpu::get(gpu);
    auto vkDev = g.getVkDevice();
    getCreateInfo(g, &ci, w, h, z);
    VK_ASSERT(vkDev.createImage(&ci, ALLOCCB, ret), "Failed to create image");
    vk::MemoryRequirements mr;
    vkDev.getImageMemoryRequirements(*ret, &mr);
    VRam* vram = mem.getPtr(gpu);
    g.allocate(mr, vram);
    vkDev.bindImageMemory(*ret, *vram, 0);
  };

  void ImageBase::destroyImage(vk::Image* d, size_t gpuIdx) {//static
    Gpu& gpu = Gpu::get(gpuIdx);
    auto vkDev = gpu.getVkDevice();
    vkDev.destroyImage(*d);//NOTE vram deallocated by VRam destructor
  };

  void ImageBase::makeStateTracker(StateSynchronizer<accessState>* ret, size_t gpu) {
    new(ret)StateSynchronizer<accessState>({
	vk::ImageLayout::eUndefined,
	vk::AccessFlagBits2::eNone,
	vk::PipelineStageFlagBits2::eNone,
	0 //which queue initially owns the image is unclear
      },
      StateSynchronizer<accessState>::changeState_F::make<ImageBase, size_t>(this, gpu, &ImageBase::updateAccessState));
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
			       slotData.shouldBeLinear() ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal,
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

  vk::Image ImageBase::getVkImage(size_t gpuIdx) {
    return images[gpuIdx];
  };

  vk::ImageView ImageBase::makeVkImageView(size_t gpu) {
    auto vkDev = Gpu::get(gpu).getVkDevice();
    vk::ImageViewCreateInfo ivci { {}, getVkImage(gpu), getViewTypeForWhole(slotData), format, {}, {
	aspectMaskForUsage(slotData.usage), 0, slotData.mipLevels, 0, slotData.arrayLayers } };
    vk::ImageView ret;
    VK_ASSERT(vkDev.createImageView(&ivci, ALLOCCB, &ret), "Failed to create image view");
    return ret;
  };

  void ImageBase::makeAccessors(accessObject* ret, size_t gpu) {
    auto vkDev = Gpu::get(gpu).getVkDevice();
    ret->view = makeVkImageView(gpu);
    vk::SamplerCreateInfo sci { {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
      vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToEdge,
      0, true, 1.0f, false, vk::CompareOp::eNever, 0, static_cast<float>(slotData.mipLevels),
      vk::BorderColor::eFloatTransparentBlack, false };
    //TODO research and maybe options for filter, anisotropic filter, compare
    VK_ASSERT(vkDev.createSampler(&sci, ALLOCCB, &ret->sampler), "Failed to create sampler");
    ret->dsImageInfo = { ret->sampler, ret->view, optimalLayoutForUsage(slotData.externalUsage) };
  };

  void ImageBase::destroyAccessors(accessObject* doomed, size_t gpu) {//static
    auto vkDev = Gpu::get(gpu).getVkDevice();
    vkDev.destroyImageView(doomed->view, ALLOCCB);
    vkDev.destroySampler(doomed->sampler, ALLOCCB);
  };

  void ImageBase::updateAccessState(size_t gpuIdx, accessState old, accessState* gnu, WorkBatch& wb) {
    if(old.layout == gnu->layout && old.queueFam == gnu->queueFam) {
      //we don't actually need a memory barrier to change the others,
      //just need to or them in so the future transition uses the write stages and accesses
      gnu->accessMask |= old.accessMask;
      gnu->stageMask |= old.stageMask;
      return;
    }
    vk::ImageMemoryBarrier2 mb {
      old.stageMask, old.accessMask, gnu->stageMask, gnu->accessMask, old.layout, gnu->layout, old.queueFam, gnu->queueFam,
      getVkImage(gpuIdx), getAllInclusiveSubresource()
    };
    vk::DependencyInfo di;
    di.pImageMemoryBarriers = &mb;
    di.imageMemoryBarrierCount = 1;
    wb.pipelineBarrier2(&di);
  };

  vk::ImageAspectFlags ImageBase::getAspects() {
    return slotData.usage & USAGE_ATT_DEPTH ? vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eColor;
  };

  vk::ImageSubresourceRange ImageBase::getAllInclusiveSubresource() {
    return { getAspects(), 0, slotData.mipLevels, 0, slotData.arrayLayers };
  };

  vk::ImageSubresourceLayers ImageBase::getAllSubresourceLayers() {
    return { getAspects(), 0, 0, slotData.arrayLayers };
  };

  vk::ImageView ImageBase::getVkImageView(size_t gpuIdx) {
    return accessors[gpuIdx]->view;
  };

  void ImageBase::populateDSWrite(vk::WriteDescriptorSet* out, size_t gpuIdx) {
    out->pImageInfo = &accessors[gpuIdx]->dsImageInfo;
    out->descriptorCount = 1;
    out->dstArrayElement = 0;
  };

  void ImageBase::ensureExists(size_t gpu) {
    images.getPtr(gpu);
  };

  size_t ImageBase::getMemSize(size_t gpu) {
    ensureExists(gpu);
    return mem.getRef(gpu).mai.allocationSize;
  }

}
