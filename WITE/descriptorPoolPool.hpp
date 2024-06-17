/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <vector>
#include <queue>

#include "wite_vulkan.hpp"
#include "literalList.hpp"
#include "gpu.hpp"

namespace WITE {

  //NOTE thread safe.
  struct descriptorPoolPoolBase {
  public:
    descriptorPoolPoolBase() = default;
    virtual ~descriptorPoolPoolBase() = default;
    virtual vk::DescriptorSet allocate() = 0;
    virtual void free(vk::DescriptorSet f) = 0;
  };

  template<literalList<resourceConsumer> allRCS, uint64_t GPUID>
  struct descriptorPoolPool : public descriptorPoolPoolBase {

    struct isDescriptor {
      constexpr bool operator()(resourceConsumer rc) {
	return rc.usage.type == resourceUsageType::eDescriptor;
      };
    };
    static constexpr auto RCS = where<resourceConsumer, allRCS, isDescriptor>();
    static constexpr uint32_t batchSize = 256;
    static constexpr copyableArray<vk::DescriptorSetLayoutBinding, RCS.LENGTH> bindings = [](size_t i) {
      return vk::DescriptorSetLayoutBinding { uint32_t(i), RCS[i].usage.asDescriptor.descriptorType, 1, RCS[i].stages };
    };
    static constexpr vk::DescriptorSetLayoutCreateInfo dslci { {}, RCS.LENGTH, bindings.ptr() };
    static constexpr copyableArray<vk::DescriptorPoolSize, RCS.LENGTH> poolSizes = [](size_t i) {
      return vk::DescriptorPoolSize { RCS[i].usage.asDescriptor.descriptorType, 1 };
    };
    static constexpr vk::DescriptorPoolCreateInfo dpci { {}, batchSize, RCS.LENGTH, poolSizes.ptr() };

  private:
    gpu& dev;
    vk::DescriptorSetLayout layoutStaging[batchSize];//N copies of the same layout
    vk::DescriptorSet setsStaging[batchSize];
    vk::DescriptorSetAllocateInfo allocInfo;
    std::vector<vk::DescriptorPool> pools;
    std::queue<vk::DescriptorSet> available;

  public:
    descriptorPoolPool() : dev(gpu::get(GPUID)) {
      vk::DescriptorSetLayout layout = dev.getDescriptorSetLayout<dslci>();
      memset<batchSize>(layoutStaging, layout);
      allocInfo.setDescriptorSetCount(batchSize)
	.setPSetLayouts(layoutStaging);
    };

    virtual ~descriptorPoolPool() {
      for(vk::DescriptorPool p : pools)
	dev.getVkDevice().destroyDescriptorPool(p, ALLOCCB);
    };

    //vulkan pools are fixed size. Rather than tracking which pools have available DS, we simply immediately exhaust new pools into the available buffer and never give them back to vulkan
    virtual vk::DescriptorSet allocate() override {
      if constexpr(RCS.LENGTH) {
	vk::DescriptorSet ret;
	if(available.size()) {
	  ret = available.front();
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
      } else {
	return VK_NULL_HANDLE;
      }
    };

    virtual void free(vk::DescriptorSet f) override {
      if constexpr(RCS.LENGTH) {
	available.push(f);
      }
    };

  };

  template<uint64_t GPUID> struct descriptorPoolPool<literalList<resourceConsumer>{}, GPUID> : public descriptorPoolPoolBase {
    static constexpr vk::DescriptorSetLayoutCreateInfo dslci { {}, 0, NULL };
    vk::DescriptorSet allocate() override { return VK_NULL_HANDLE; };
    void free(vk::DescriptorSet) override {};
  };

};
