#pragma once

#include <vector>
#include <queue>

#include "wite_vulkan.hpp"

namespace WITE {

  //NOTE thread safe.
  struct descriptorPoolPoolBase {
  public:
    descriptorPoolPoolBase() = default;
    virtual ~descriptorPoolPoolBase() = default;
    virtual vk::DescriptorSet allocate() = 0;
    virtual void free(vk::DescriptorSet f) = 0;
  };

  template<literalList<resourceReference> RRS>
  struct descriptorPoolPool : public descriptorPoolPool {
  private:

    static constexpr uint32_t batchSize = 256;
    static constexpr copyableArray<vk::DescriptorSetLayoutBinding, RRS.len> bindings = [](size_t i) {
      return vk::DescriptorSetLayoutBinding { uint32_t(i), RRS[i].descriptorType, 1, RRS[i].stages };
    };
    static constexpr vk::DescriptorSetLayoutCreateInfo dslci { {}, RRS.len, bindings.ptr() };
    static constexpr copyableArray<vk::DescriptorPoolSize, RRS.len> poolSizes = [](size_t i) {
      return vk::DescriptorPoolSize { RRS[i].descriptorType, 1 };
    };
    static constexpr vk::DescriptorPoolCreateInfo dpci { {}, batchSize, RRS.len, poolSizes.ptr() };
    gpu& dev;
    vk::DescriptorSetLayout layoutStaging[batchSize];//N copies of the same layout
    vk::DescriptorSet setsStaging[batchSize];
    vk::DescriptorSetAllocateInfo allocInfo;
    std::vector<vk::DescriptorPool> pools;
    std::queue<vk::DescriptorSet> available;

  public:
    descriptorPoolPool() {
      dev = gpu::get(GPUID);
      vk::DescriptorSetLayout layout;
      VK_ASSERT(dev.getVkDevice().createDescriptorSetLayout(&dslci, ALLOCCB, &layout), "failed to create descriptor set layout");
      memset<batchSize>(layoutStaging, layout);
      allocInfo.setDescriptorSetCount(batchSize)
	.setPSetLayouts(layoutStaging);
    };

    virtual ~descriptorPoolPool() {
      for(vk::DescriptorPool p : pools)
	dev.destroyDescriptorPool(p, ALLOCCB);
    };

    //vulkan pools are fixed size. Rather than tracking which pools have available DS, we simply immediately exhaust new pools into the available buffer and never give them back to vulkan
    virtual vk::DescriptorSet allocate() override {
      vk::DescriptorSet ret;
      if(available.size()) {
	ret = available.top();
	available.pop();
      } else {
	vk::DescriptorPool newPool;
	VK_ASSERT(dev.getVkDevice().createDescriptorPool(&dpci, ALLOCCB, &newPool), "failed to create descriptor set pool");
	pools.push_back(newPool);
	allocInfo.setDescriptorPool(newPool);
	VK_ASSERT(dev.getVkDevice().allocateDescriptorSets(&allocInfo, setsStaging), "failed to empty pool");
	ret = setsStaging[0];
	for(size_t i = 1;i < batchSize;i++)
	  available.push(setsStaging[i]);
      }
      return ret;
    };

    virtual void free(vk::DescriptorSet f) override {
      available.push(f);
    };

  };

};
