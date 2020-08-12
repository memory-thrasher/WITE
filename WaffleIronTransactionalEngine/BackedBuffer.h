#pragma once

#include "stdafx.h"
#include "GPU.h"

class BackedImage;

class BackedBuffer : public WITE::ShaderResource
{
public:
  BackedBuffer(GPU* dev, VkMemoryRequirements* memReqs, VkFlags heapflags = 0, VkFlags typeflags = 0);
  BackedBuffer(GPU* dev, size_t size, VkBufferUsageFlags usage);
  ~BackedBuffer();
  void bindToImage(VkImage);
  void* map();
  void unmap();
  size_t getSize();
  void load(WITE::rawDataSource);
  BackedBuffer* getBuffer() { return this; };
private:
  VkDescriptorBufferInfo info;
  VkBuffer buffer = VK_NULL_HANDLE;
  size_t size;
  VkDeviceMemory mem;
  GPU* dev;
  void* cachedMap = NULL;
  friend class BackedImage;
  friend class Renderer;
};

