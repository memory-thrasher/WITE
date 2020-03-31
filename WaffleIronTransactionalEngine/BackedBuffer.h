#pragma once

#include "stdafx.h"
#include "GPU.h"

class VBackedImage;

class VBackedBuffer : public ShaderResource
{
public:
	VBackedBuffer(GPU* dev, VkMemoryRequirements* memReqs, VkFlags heapflags = 0, VkFlags typeflags = 0);
	VBackedBuffer(GPU* dev, size_t size, VkBufferUsageFlags usage);//TODO queues?
	~VBackedBuffer();
	void bindToImage(VkImage);
	unsigned char* map();
	void unmap();
	size_t getSize();
	void load(rawDataSource);
	VBackedBuffer* getBuffer() { return this; };
private:
	VkDescriptorBufferInfo getDesc();
	VkBuffer buffer = NULL;
	size_t size;
	VkDeviceMemory mem;
	GPU* dev;
	void* cachedMap = NULL;
	friend class VBackedImage;
};

