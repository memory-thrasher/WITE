#include "Buffer.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  BufferBase::BufferBase(BufferSlotData bsd) :
    buffers(PerGpu<vk::Buffer>::creator_t_F::make(this, &BufferBase::makeBuffer),
	    PerGpu<vk::Buffer>::destroyer_t_F::make(&BufferBase::destroyBuffer)),
    slotData(bsd),
    transfersRequired(Gpu::gpuCountByLdm(bsd.logicalDeviceMask) > 1)
  {};

  void BufferBase::getCreateInfo(Gpu& gpu, vk::BufferCreateInfo* out) {
    //assume this will be used with the same queues the gpu has chosen
    auto usage = slotData.usage;
    uint32_t queues[3];
    uint32_t qCount = 0;
    if(usage & USAGE_GRAPHICS) queues[qCount++] = gpu.getGraphics()->family;
    if(usage & USAGE_COMPUTE) queues[qCount++] = gpu.getCompute()->family;
    if(usage & (USAGE_HOST_READ | USAGE_HOST_WRITE | USAGE_TRANSFER)) queues[qCount++] = gpu.getTransfer()->family;
    if(qCount == 3 && (queues[0] == queues[1] || queues[2] == queues[1])) {
      queues[1] = queues[2];
      qCount = 2;
    }
    if(qCount > 1 && queues[0] == queues[qCount-1]) {
      qCount--;
    }
    bool dsWrite = usage & USAGE_DS_WRITE;
#define MAPU(x, r) ((usage & USAGE_ ##x) ? vk::BufferUsageFlagBits::e ##r : (vk::BufferUsageFlags)0)
#define MAPRW(x, r, w) ((usage & USAGE_ ##x) ? (dsWrite ? vk::BufferUsageFlagBits::e ##r : vk::BufferUsageFlagBits::e ##w) : (vk::BufferUsageFlags)0)
    *out = { {}, slotData.size,
      MAPU(TRANSFER, TransferSrc) | MAPU(TRANSFER, TransferDst) |
      MAPU(HOST_READ, TransferSrc) | MAPU(HOST_WRITE, TransferDst) |
      MAPU(DS_READ, UniformBuffer) | MAPU(DS_WRITE, StorageBuffer) | MAPRW(DS_SAMPLED, UniformTexelBuffer, StorageTexelBuffer) |
      MAPU(VERTEX, VertexBuffer) | MAPU(INDIRECT, IndirectBuffer),
      qCount > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive, qCount, queues };
#undef MAPU
#undef MAPRW
  };

  void BufferBase::makeBuffer(vk::Buffer* ret, size_t gpuIdx) {
    vk::BufferCreateInfo ci;
    Gpu& gpu = Gpu::get(gpuIdx);
    getCreateInfo(gpu, &ci);
    auto vkDev = gpu.getVkDevice();
    VK_ASSERT(vkDev.createBuffer(&ci, ALLOCCB, ret), "Failed to create buffer");
    vk::MemoryRequirements mr;
    vkDev.getBufferMemoryRequirements(*ret, &mr);
    VRam* vram = &mem.getRef(gpuIdx);
    gpu.allocate(mr, vram);
    vkDev.bindBufferMemory(*ret, *vram, 0);
  };

  vk::Buffer BufferBase::getVkBuffer(size_t gpuIdx) {
    return buffers[gpuIdx];
  };

  void BufferBase::destroyBuffer(vk::Buffer* d, size_t gpu) {//static
    Gpu::get(gpu).getVkDevice().destroyBuffer(*d);
  };

};
