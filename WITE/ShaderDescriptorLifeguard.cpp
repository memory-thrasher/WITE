#include "ShaderDescriptorLifeguard.hpp"
#include "DEBUG.hpp"

namespace WITE::GPU {

  Collections::PerGpuPerThread<std::unique_ptr<std::map<ShaderData::hashcode_t, ShaderDescriptorLifeguard>>> all; //static

  ShaderDescriptorLifeguard::ShaderDescriptorLifeguard(const vk::DescriptorSetLayoutCreateInfo* dslCI,
						       const vk::DescriptorPoolCreateInfo* poolCI,
						       size_t gpuIdx) :
    poolCI(poolCI),
    gpu(Gpu::get(gpuIdx))
  {
    auto vkDev = gpu.getVkDevice();
    VK_ASSERT(vkDev.createDescriptorSetLayout(dslCI, ALLOCCB, &dsl), "Could not create DS layout");
    allocatePool();
  };

  void ShaderDescriptorLifeguard::allocatePool() {
    vk::DescriptorPool pool;
    VK_ASSERT(gpu.getVkDevice().createDescriptorPool(poolCI, ALLOCCB, &pool), "failed to allocate descriptor pool");
    poolPool.push_back(pool);
  };

  vk::DescriptorSet ShaderDescriptorLifeguard::allocate() {
    vk::DescriptorSet ret;
    if(!free.empty()) {
      ret = free.top();
      free.pop();
      return ret;
    }
    auto vkDev = gpu.getVkDevice();
    vk::DescriptorSetAllocateInfo allocInfo { poolPool[currentPool], 1, &dsl };
    vk::Result res = vkDev.allocateDescriptorSets(&allocInfo, &ret);
    if(res == vk::Result::eSuccess) return ret;
    if(res == vk::Result::eErrorOutOfPoolMemory) {
      currentPool++;
      allocatePool();
      allocInfo.descriptorPool = poolPool[currentPool];
      VK_ASSERT(vkDev.allocateDescriptorSets(&allocInfo, &ret), "Failed to allocate descriptor set from fresh pool");
    } else {
      CRASHRET(VK_NULL_HANDLE, "Failed to allocate descriptor set ", res);
    }
    return ret;
  };

  void ShaderDescriptorLifeguard::deallocate(vk::DescriptorSet r) {
    free.push(r);
  };

};
