#pragma once

#include "stdafx.h"
#include "VBackedBuffer.h"

/*
  TODO
  provide simplified usage on creation, determines VK_ACCESS_*_BIT usages
  allocate staging more dynamically
*/

class VBackedImage : public ShaderResource
{
public:
  VBackedImage(GPU*, VkExtent2D, VkImageViewCreateInfo = {}, VkImageCreateInfo = {});
  VBackedImage(GPU*, VkExtent2D, VkFormat, VkImage = NULL, uint32_t mipmap = 1);
  ~VBackedImage();
  void makeSampler();
  void load(VBackedBuffer* src, size_t offset = 0, size_t srcBufWidth = -1, size_t srcBufHeight = -1,
	    size_t dstX = 0, size_t dstY = 0, size_t dstWidth = -1, size_t dstHeight = -1);
  void load(rawDataSource);
  VkFormat getFormat() { return format; };
  VkImageView getImageView() { return view; };
  VBackedBuffer* getBuffer() { return backing; };
  size_t getSize() { return backing->getSize(); };
  void unmap() { (staging ? staging : backing)->unmap(); };
  unsigned char* map() { return (staging ? staging : backing)->map(); };
  VkImage getImage() { return image; };
private:
  VkImage image;
  VkImageView view;
  VkExtent2D size;
  VkImageViewCreateInfo viewInfo;
  VkFormat format;
  GPU* dev;
  //the previous are all that are required for swapchain images, move to non-backed image base class?
  VkImageCreateInfo imageInfo;
  VkFormatProperties props;
  VkMemoryRequirements memReqs;//TODO move to buffer class?
  VBackedBuffer *backing = NULL, *staging = NULL;
  VkDescriptorImageInfo info;
  VkSamplerCreateInfo samplerInfo;
  VkSampler sampler = NULL;
};

