#include "ShaderDescriptorLifeguard.hpp"
#include "DEBUG.hpp"

namespace WITE::GPU {

  Collections::PerGpuPerThread<std::map<ShaderData::hashcode_t, ShaderDescriptorLifeguard>> all; //static

  ShaderDesctorLifeguard::ShaderDesctorLifeguard(const vk::DescriptorSetLayoutCreateInfo* dslCI,
						 const vk::DescriptorPoolCreateInfo* poolCI,
						 size_t gpuIdx) :
    poolCi(poolCI),
    gpu(&Gpu::get(gpuIdx))
  {
    auto vkDev = gpu.getVkDevice();
    VK_ASSERT(vkDev.createDescriptorSetLayout(dslCI, ALLOCCB, &dsl), "Could not create DS layout");
    allocatePool();
  };

  void ShaderDesctorLifeguard::allocatePool() {
    vk::DescriptorPool pool;
    VK_ASSERT(gpu->getVkDevice().createDescriptorPool(poolCI, ALLOCCB, &pool), "failed to allocate descriptor pool");
    poolPool.push_back(pool);
  };

  vk::DescriptorSet ShaderDesctorLifeguard::allocate() {
    vk::DescriptorSet ret;
    if(!free.empty()) {
      ret = free.top();
      free.pop();
      return ret;
    }
    auto vkDev = gpu->getVkDevice();
    vk::DescriptorSetAllocateInfo allocInfo { sdl.poolPool[sdl.currentPool], 1, &sdl.dsl };
    VkResult res = vkDev.allocateDescriptorSets(&allocInfo, &ret);
    if(res == vk::Result::eSuccess) return ret;
    if(res == vk::Result::eErrorOutOfPoolMemory) {
      sdl.currentPool++;
      sdl.allocatePool();
      allocInfo.descriptorPool = sdl.poolPool[sdl.currentPool];
      VK_ASSERT(vkDev.allocateDescriptorSets(&allocInfo, &ret), "Failed to allocate descriptor set from fresh pool");
    } else {
      CRASHRET(VK_NULL_HANDLE, "Failed to allocate descriptor set ", res);
    }
    return ret;
  };

  void free(vk::DescriptorSet r) {
    free.push(r);
  };

};
