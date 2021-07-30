#include "Internal.h"

namespace WITE_internal {

  BackedImage::BackedImage(GPU* dev, VkExtent2D sizeParam, VkImageViewCreateInfo viewInfo, VkImageCreateInfo imgInfo) :
    viewInfo(viewInfo), format(viewInfo.format), dev(dev) {
    vkGetPhysicalDeviceFormatProperties(dev->phys, format, &props);
    if (!this->viewInfo.image) {
      bool staged = !(props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
      if (imgInfo.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO) {
	imageInfo = imgInfo;
	sizeParam = { imgInfo.extent.width, imgInfo.extent.height };
      } else {
	imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D,
		      format, { sizeParam.width, sizeParam.height, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT,
		      staged ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR,
		      (staged ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0u) | VK_IMAGE_USAGE_SAMPLED_BIT,
		      VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		      staged ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PREINITIALIZED };
      }
      CRASHIFFAIL(vkCreateImage(dev->device, &imageInfo, vkAlloc, &image));
      vkGetImageMemoryRequirements(dev->device, image, &memReqs);
      if (staged) {
	staging = new BackedBuffer(dev, memReqs.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	backing = new BackedBuffer(dev, &memReqs, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);
      } else {
	backing = new BackedBuffer(dev, &memReqs, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);
      }
      backing->bindToImage(image);
      this->viewInfo.image = image;
    } else {
      image = viewInfo.image;
    }
    this->size = sizeParam;
    vkCreateImageView(dev->device, &this->viewInfo, vkAlloc, &view);
  }

  BackedImage::BackedImage(GPU* dev, VkExtent2D size, VkFormat format, VkImage image, uint32_t mipmap) :
    BackedImage(dev, size, { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, image, VK_IMAGE_VIEW_TYPE_2D, format, SWIZZLE_IDENTITY, subres0 }) {
    //when image is provided, the caller is responsible for destroying it
    //general use case is swapchain images, in which case they are destroyed by vkDestroySwapchain
    cleanupImage = false;
  }

  BackedImage::BackedImage(GPU* dev, uint32_t width, uint32_t height, uint32_t Wformat, uint64_t imageUsages, uint32_t mipmap) : size({width, height}), format(static_cast<VkFormat>(Wformat)), dev(dev) {
    bool isDepth = format == FORMAT_STD_DEPTH;
    uint32_t vkUsage = 0;
    if(imageUsages & USAGE_ATTACH_AUTO)
      vkUsage |= isDepth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if(imageUsages & USAGE_TRANSFER_SRC)
      vkUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if(imageUsages & USAGE_TRANSFER_DST)
      vkUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if(imageUsages & USAGE_SAMPLED)
      vkUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    vkGetPhysicalDeviceFormatProperties(dev->phys, format, &props);
    bool staged = bool(imageUsages & USAGE_LOAD) && !(props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    if(staged)
      vkUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    //TODO cross-gpu transfer option, with queue sharing
    imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D, format, { width, height, 1 }, mipmap, 1, VK_SAMPLE_COUNT_1_BIT, staged ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR, vkUsage, VK_SHARING_MODE_EXCLUSIVE, 0, NULL, staged ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PREINITIALIZED };
    CRASHIFFAIL(vkCreateImage(dev->device, &imageInfo, vkAlloc, &image));
    if(staged) {
      staging = new BackedBuffer(dev, memReqs.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
      backing = new BackedBuffer(dev, &memReqs, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);
    } else {
      backing = new BackedBuffer(dev, &memReqs, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);
    }
    backing->bindToImage(image);
    viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, image, VK_IMAGE_VIEW_TYPE_2D, format, {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A}, {isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT, 0, mipmap, 0, 1}};
    vkCreateImageView(dev->device, &this->viewInfo, vkAlloc, &view);
  }

  BackedImage::~BackedImage() {
    VkDevice dev = this->dev->device;
    if(sampler) vkDestroySampler(dev, sampler, NULL);
    if(view) vkDestroyImageView(dev, view, NULL);
    if(cleanupImage && image) vkDestroyImage(dev, image, NULL);
    if(backing) delete backing;
    if(staging) delete staging;
  }

  //If an image is being loaded from the cpu, I'm just going to assume that a shader will want to read it.
  void BackedImage::load(WITE::rawDataSource push) {
    (staging ? staging : backing)->load(push);
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//seems valid, if being loaded from cpu
    if (!staging) {
      VkCommandBuffer cmd = dev->transferQ->makeCmd();
      //vkGetImageSubresourceLayout(dev->device, image, &subres, &layout);
      VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
						  VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED,
						  info.imageLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
						  image, viewInfo.subresourceRange };
      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			   0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
      dev->transferQ->submit(cmd);
      dev->transferQ->destroyCmd(cmd);
    } else {
      load(staging);
    }
  }

  void BackedImage::load(BackedBuffer* src, size_t o, size_t bw, size_t bh, size_t dx, size_t dy,
			 size_t dw, size_t dh) {
    //TODO if src->gpu != gpu, stage first
    if (bw > size.width) bw = size.width;
    if (bh > size.height) bh = size.height;
    if (dw > size.width) dw = size.width;
    if (dh > size.height) dh = size.height;
    VkBufferImageCopy copyRegion = { o, (uint32_t)bw, (uint32_t)bh, { viewInfo.subresourceRange.aspectMask, 0, 0, 1 },
				     { (int32_t)dx, (int32_t)dy, 0 }, { (uint32_t)dw, (uint32_t)dh, 0 } };
    VkCommandBuffer cmd = dev->transferQ->makeCmd();
    VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, viewInfo.subresourceRange };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    vkCmdCopyBufferToImage(cmd, src->buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, viewInfo.subresourceRange };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    dev->transferQ->submit(cmd);
    dev->transferQ->destroyCmd(cmd);
  }

  void BackedImage::makeSampler() {
    samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, NULL, 0, VK_FILTER_NEAREST,
		    VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		    0.0, VK_FALSE, 1, VK_FALSE, VK_COMPARE_OP_NEVER, 0, 0, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		    VK_FALSE };
    CRASHIFFAIL(vkCreateSampler(dev->device, &samplerInfo, vkAlloc, &sampler));
  }

  size_t BackedImage::getWidth() {
    return size.width;
  }

  size_t BackedImage::getHeight() {
    return size.height;
  }

  static auto pickAccessByLayout(int32_t layout) {
    //deduce access flags based on new layout
    decltype(vkAccessBitmask) ret = 0;
    switch(layout) {
    case LAYOUT_TRANSFER_DST:
      ret |= VK_ACCESS_TRANSFER_WRITE_BIT;
      break;
    }
    return ret;
  }

  void BackedImage::setLayout(int32_t newLayout, VkCmdBuffer cmd, Queue* newOwner) {
    //TODO pipeline stage?
    //TODO pick aspect? (why would you transition a depth aspect?... stencil?)
    auto newAccess = pickAccessByLayout(newLayout);
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
				     vkAccessBitmask, newAccess,
				     layout, newLayout,
				     ownerQueue && newOwner ? ownerQueue->family : VK_QUEUE_FAMILY_IGNORED, newOwner->newOwner->family : VK_QUEUE_FAMILY_IGNORED,
				     image,
				     { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    vkAccessBitmask = newAccess;
    layout = newLayout;
    if(newOwner) ownerQueue = newOwner;
  }

  void BackedImage::discard() {
    layout = VK_IMAGE_LAYOUT_UNDEFINED;//see setLayout
    ownerQueue = NULL;//??
  }

  void BackedImage::clear(VkCmdBuffer cmd, const VkClearColorValue *color) {
    if(layout != LAYOUT_TRANSFER_DST) {
      discard();
      setLayout(LAYOUT_TRANSFER_DST, cmd);
    }
    vkCmdClearColorImage(cmd, image, layout, color, 1, &subres0);
  }

}

std::unique_ptr<WITE::BackedImage> WITE::BackedImage::make(WITE::GPU* dev, uint32_t width, uint32_t height, uint32_t Wformat, uint64_t imageUsages, uint32_t mipmap) {
  return std::make_unique(reinterpret_cast<Wite_internal::GPU*>(dev), width, height, Wformat, imageUsages, mipmap);
}
