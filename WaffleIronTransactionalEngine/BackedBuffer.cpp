#include "stdafx.h"
#include "BackedBuffer.h"
#include "Export.h"

constexpr static uint32_t PICK_MEMORY_FAILED = std::numeric_limits<uint32_t>::max();

uint32_t pickMemType(VkMemoryRequirements* memReqs, GPU* gpu, VkFlags heapFlags, VkFlags typeFlags) {
  uint32_t ret = PICK_MEMORY_FAILED, i, j;
  for (i = 0, j = 1;j < memReqs->memoryTypeBits && ret == PICK_MEMORY_FAILED;i++, j = j << 1)
    if ((j & memReqs->memoryTypeBits) &&
	(gpu->memProps.memoryTypes[i].propertyFlags & typeFlags) == typeFlags && heapFlags ==
	(gpu->memProps.memoryHeaps[gpu->memProps.memoryTypes[i].heapIndex].flags & heapFlags) &&
	gpu->memProps.memoryHeaps[gpu->memProps.memoryTypes[i].heapIndex].size >= memReqs->size)
      ret = i;
  if (ret == PICK_MEMORY_FAILED) LOG("Warn: failed to find appropriate heap\n");
  return ret;
}

BackedBuffer::BackedBuffer(GPU* dev, VkMemoryRequirements* memReqs, VkFlags heapflags, VkFlags typeflags) : size(memReqs->size), dev(dev) {
  VkMemoryAllocateInfo minfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, memReqs->size,
	   pickMemType(memReqs, dev, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, 0) };
  CRASHIFFAIL(vkAllocateMemory(dev->device, &minfo, vkAlloc, &mem));//TODO split this off?function/class?
  info = { buffer, 0, size };
}

BackedBuffer::BackedBuffer(GPU* dev, size_t size, VkBufferUsageFlags usage) : size(size), dev(dev) {
  VkBufferCreateInfo bufferInfo;
  bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, size, usage, VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
  CRASHIFFAIL(vkCreateBuffer(dev->device, &bufferInfo, vkAlloc, &buffer));
  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(dev->device, buffer, &memReqs);
  VkMemoryAllocateInfo minfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, memReqs.size,
				pickMemType(&memReqs, dev, 0, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
  CRASHIFFAIL(vkAllocateMemory(dev->device, &minfo, vkAlloc, &mem));
  CRASHIFFAIL(vkBindBufferMemory(dev->device, buffer, mem, 0));
  info = { buffer, 0, size };
}

BackedBuffer::~BackedBuffer() {
  //vkDestroyBuffer(dev->device, buffer, NULL);
  //vkFreeMemory(dev->device, mem, NULL);
}

size_t BackedBuffer::getSize() {
  return size;
}

void BackedBuffer::bindToImage(VkImage image) {
  CRASHIFFAIL(vkBindImageMemory(dev->device, image, mem, 0));
}

void* BackedBuffer::map() {
  if(!cachedMap)
    CRASHIFFAIL(vkMapMemory(dev->device, mem, 0, size, 0, &cachedMap), NULL);
  return cachedMap;
}

void BackedBuffer::unmap() {
  vkUnmapMemory(dev->device, mem);//dispose? reusable?
  cachedMap = NULL;
}

void BackedBuffer::load(WITE::rawDataSource src) {
  src->call(map(), size);
  unmap();
}
