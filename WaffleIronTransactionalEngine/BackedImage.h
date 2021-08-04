#pragma once

/*
  TODO
  provide simplified usage on creation, determines VK_ACCESS_*_BIT usages
  allocate staging more dynamically
  use ownerQueue more completely
*/

class BackedImage : public WITE::BackedImage {
public:
  const VkImageSubresourceRange subres0 = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
  const VkImageSubresourceRange subres0_depth = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
  const VkComponentMapping SWIZZLE_IDENTITY = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
  BackedImage(GPU*, VkExtent2D, VkImageViewCreateInfo = {}, VkImageCreateInfo = {});
  BackedImage(GPU*, VkExtent2D, VkFormat, VkImage = VK_NULL_HANDLE, uint32_t mipmap = 1);
  BackedImage(GPU*, uint32_t width, uint32_t height, uint32_t format, uint64_t imageUsages = USAGE_SAMPLED, uint32_t mipmap = 1);
  ~BackedImage();
  void load(BackedBuffer* src, size_t offset = 0, size_t srcBufWidth = -1, size_t srcBufHeight = -1,
	    size_t dstX = 0, size_t dstY = 0, size_t dstWidth = -1, size_t dstHeight = -1);
  void load(WITE::rawDataSource);
  inline VkFormat getFormat() { return format; };
  inline VkImageView getImageView() { return view; };
  inline size_t getSize() { return backing->getSize(); };
  uint64_t serialNumber() { return _serialNumber; };
  size_t getWidth();
  size_t getHeight();
  inline void unmap() { (staging ? staging : backing)->unmap(); };
  inline void* map() { return (staging ? staging : backing)->map(); };
  inline VkImage getImage() { return image; };
  int32_t getLayout() { return layout; };
  void setLayout(int32_t layout, VkCmdBuffer cmd, Queue* newOwner = NULL);
  void discard();//no immediate gpu operation, should be followed-up by a setLayout
  void clear(VkCmdBuffer cmd, const VkClearColorValue *color);
private:
  static uint64_t serialNumberSeed = 0;
  static int32_t pickAccessByLayout(int32_t layout);
  uint64_t _serialNumber = serialNumberSeed++;
  VkImage image;
  VkImageView view;
  VkExtent2D size;
  VkImageViewCreateInfo viewInfo;
  VkFormat format;
  int32_t layout;
  int32_t vkAccessBitmask;
  GPU* dev;
  Queue* ownerQueue;
  bool cleanupImage = true;
  //the previous are all that are required for swapchain images, move to non-backed image base class?
  VkImageCreateInfo imageInfo;
  VkFormatProperties props;
  VkMemoryRequirements memReqs;//TODO move to buffer class?
  BackedBuffer *backing = NULL, *staging = NULL;
  VkDescriptorImageInfo info;
  VkSamplerCreateInfo samplerInfo;
  VkSampler sampler = VK_NULL_HANDLE;
  friend class Renderer;
};

