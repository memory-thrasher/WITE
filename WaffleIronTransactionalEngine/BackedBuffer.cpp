#include "stdafx.h"
#include "VBackedBuffer.h"
#include "Export.h"

uint32_t pickMemType(VkMemoryRequirements* memReqs, GPU* gpu, VkFlags heapFlags, VkFlags typeFlags) {
	uint32_t ret = -1, i, j;
	for (i = 0, j = 1;j < memReqs->memoryTypeBits && ret == -1;i++, j = j << 1)
		if ((j & memReqs->memoryTypeBits) &&
			(gpu->memProps.memoryTypes[i].propertyFlags & typeFlags) == typeFlags && heapFlags ==
			(gpu->memProps.memoryHeaps[gpu->memProps.memoryTypes[i].heapIndex].flags & heapFlags) &&
			gpu->memProps.memoryHeaps[gpu->memProps.memoryTypes[i].heapIndex].size >= memReqs->size)
			ret = i;
	if (ret == -1) LOG("Warn: failed to find appropriate heap\n");
	return ret;
}

VBackedBuffer::VBackedBuffer(GPU* dev, VkMemoryRequirements* memReqs,
	VkFlags heapflags, VkFlags typeflags) : dev(dev), size(memReqs->size) {
	VkMemoryAllocateInfo info;
	info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, memReqs->size,
		pickMemType(memReqs, dev, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, 0) };
	CRASHIFFAIL(vkAllocateMemory(dev->device, &info, vkAlloc, &mem));//TODO split this off?function/class?
}

VBackedBuffer::VBackedBuffer(GPU* dev, size_t size, VkBufferUsageFlags usage) : dev(dev), size(size) {
	VkBufferCreateInfo bufferInfo;
	bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, size, usage, VK_SHARING_MODE_EXCLUSIVE,
		0, NULL };
	CRASHIFFAIL(vkCreateBuffer(dev->device, &bufferInfo, vkAlloc, &buffer));
	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(dev->device, buffer, &memReqs);
	VkMemoryAllocateInfo info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, memReqs.size,
		pickMemType(&memReqs, dev, 0,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
	CRASHIFFAIL(vkAllocateMemory(dev->device, &info, vkAlloc, &mem));
	CRASHIFFAIL(vkBindBufferMemory(dev->device, buffer, mem, 0));
}

VBackedBuffer::~VBackedBuffer()
{
}

size_t VBackedBuffer::getSize() {
	return size;
}

void VBackedBuffer::bindToImage(VkImage image) {
	CRASHIFFAIL(vkBindImageMemory(dev->device, image, mem, 0));
}

unsigned char* VBackedBuffer::map() {
	if(!cachedMap)
		CRASHIFFAIL(vkMapMemory(dev->device, mem, 0, size, 0, &cachedMap), NULL);
	return static_cast<unsigned char*>(cachedMap);
}

void VBackedBuffer::unmap() {
	vkUnmapMemory(dev->device, mem);//dispose? reusable?
	cachedMap = NULL;
}

void VBackedBuffer::load(rawDataSource src) {
	src->call(map(), size);
	unmap();
}

VkDescriptorBufferInfo VBackedBuffer::getDesc() {
	return { buffer, 0, size };
}
