#include "stdafx.h"
#include "BackedImage.h"
#include "Export.h"
#include "Queue.h"

BackedImage::BackedImage(GPU* dev, VkExtent2D sizeParam, VkImageViewCreateInfo viewInfo,
			 VkImageCreateInfo imgInfo) : viewInfo(viewInfo), format(viewInfo.format), dev(dev) {
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
  BackedImage(dev, size, { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, image,
			   VK_IMAGE_VIEW_TYPE_2D, format, { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
							    VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			   { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipmap, 0, 1 } }) {
  //when image is provided, the caller is responsible for destroying it
  //general use case is swapchain images, in which case they are destroyed by vkDestroySwapchain
  cleanupImage = false;
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
  VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
					      0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
					      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					      image, viewInfo.subresourceRange };
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		       0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
  vkCmdCopyBufferToImage(cmd, staging->buffer, image,
			 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
  imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
					      VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
					      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					      image, viewInfo.subresourceRange };
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
		       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
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
