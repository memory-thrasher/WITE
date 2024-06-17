/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <map>
#include <memory>
#include <atomic>

#include "wite_vulkan.hpp"
#include "constants.hpp"
#include "stdExtensions.hpp"
#include "syncLock.hpp"
#include "hash.hpp"
#include "DEBUG.hpp"
#include "thread.hpp"
#include "cmdPool.hpp"

namespace WITE {

  class gpu {
  public:
    struct vram {
      vk::DeviceMemory handle;
      const vk::MemoryAllocateInfo mai;
      gpu* dev;
      vram() = default;//dummy for cpu-ram allocation
      vram(const vk::MemoryRequirements& mr, uint8_t type, gpu* dev);
      vram(vram&& o);
      ~vram();
      void free();
    };

  private:
    static std::atomic_bool inited;
    static vk::Instance vkInstance;
    static size_t gpuCount;
    static std::unique_ptr<gpu[]> gpus;
    static char appName[1024];

    size_t idx;
    std::map<vk::Format, vk::FormatProperties> formatProperties;
    vk::PhysicalDevice pv;
    vk::PhysicalDeviceProperties2 pvp;
    typedef struct {
      vk::QueueFamilyProperties2 p;
      vk::QueueFamilyGlobalPriorityPropertiesKHR gpp;
    } qfp_t;
    std::unique_ptr<qfp_t[]> qfp;
    vk::Device dev;
    uint32_t queueIdx = ~0;
    vk::Queue queue, lowPrioQueue;
    vk::PipelineCache pipelineCache;
    vk::PhysicalDeviceMemoryProperties2 pdmp;
    vk::PhysicalDeviceMemoryBudgetPropertiesEXT pdmbp;
    std::array<int64_t, VK_MAX_MEMORY_TYPES> memoryScoreByType;
    std::array<vk::MemoryPropertyFlags, VK_MAX_MEMORY_TYPES> memoryFlagsByType;
    std::array<std::atomic_uint64_t, VK_MAX_MEMORY_HEAPS> freeMemoryByHeap;
    std::map<hash_t, vk::Sampler> samplers;
    std::map<hash_t, vk::DescriptorSetLayout> descriptorSetLayouts;
    std::map<hash_t, vk::PipelineLayout> pipelineLayouts;
    syncLock samplersMutex, descriptorSetLayoutsMutex, pipelineLayoutsMutex, queueMutex, lowPrioQueueMutex;
    threadResource<cmdPool> tempCmds;

    gpu(size_t idx, vk::PhysicalDevice, int deviceExtensionsCount, const char** deviceExtensions);
    gpu(gpu&&) = delete;
    void recordDeallocate(vram* doomed);
  public:
    static void init(const char* appName,
		     std::initializer_list<const char*> appRequestedLayers = {},
		     std::initializer_list<const char*> appRequestedExtensions = {},
		     std::initializer_list<const char*> appRequestedDeviceExtensions = {});
    static void shutdown();
    static bool running;
    static gpu& get(size_t);
    static inline size_t getGpuCount() { return gpuCount; };
    static inline const char* getAppName() { return appName; };
    static inline auto getVkInstance() { return vkInstance; };

    gpu();//dummy
    inline size_t getIndex() { return idx; };
    inline vk::Device getVkDevice() { return dev; };//vkHandle is an opaque handle
    inline vk::Queue getQueue() { return queue; };
    inline uint32_t getQueueFam() { return queueIdx; };
    inline syncLock* getQueueMutex() { return &queueMutex; };
    inline vk::Queue getLowPrioQueue() { return lowPrioQueue; };
    inline syncLock* getLowPrioQueueMutex() { return &lowPrioQueueMutex; };
    vk::PipelineCache getPipelineCache() { return pipelineCache; };
    inline tempCmd getTempCmd() { return tempCmds.get()->allocate(); };
    inline auto getPhysical() { return pv; };
    void allocate(const vk::MemoryRequirements& mr, vk::MemoryPropertyFlags requiredFlags, vram* out);

    template<vk::DescriptorSetLayoutCreateInfo CI> vk::DescriptorSetLayout getDescriptorSetLayout() {
      scopeLock lock(&descriptorSetLayoutsMutex);
      static constexpr hash_t CIHASH = hash(CI);
      vk::DescriptorSetLayout& ret = descriptorSetLayouts[CIHASH];
      if(!ret) {
	VK_ASSERT(getVkDevice().createDescriptorSetLayout(&CI, ALLOCCB, &ret), "failed to create descriptorSetLayout");
	// WARN("Creating DSL with bindings: ", CI.bindingCount);
	// for(int i = 0;i < CI.bindingCount;i++) {
	//   const auto& b = CI.pBindings[i];
	//   WARN("  binding: ", i, " (", b.binding, "): type: ", std::hex, (long long)b.descriptorType, ", stages: ", (vk::ShaderStageFlags::MaskType)b.stageFlags, std::dec, ", count: ", b.descriptorCount);
	// }
      }
      return ret;
    };

    template<literalList<vk::DescriptorSetLayoutCreateInfo> DSLS> void populateDSLS(vk::DescriptorSetLayout* dsls) {
      if constexpr(DSLS.len) {
	*dsls = getDescriptorSetLayout<DSLS[0]>();
	populateDSLS<DSLS.sub(1)>(dsls+1);
      }
    };

    template<literalList<vk::DescriptorSetLayoutCreateInfo> DSLS> vk::PipelineLayout getPipelineLayout() {
      static constexpr hash_t PLHASH = hash(DSLS);
      scopeLock lock(&pipelineLayoutsMutex);
      vk::PipelineLayout& ret = pipelineLayouts[PLHASH];
      if(!ret) {
	vk::PipelineLayoutCreateInfo ci;//NOTE: push constants NYI, supply here if needed
	vk::DescriptorSetLayout dsls[DSLS.len];
	ci.setLayoutCount = DSLS.len;
	ci.pSetLayouts = dsls;
	populateDSLS<DSLS>(dsls);
	VK_ASSERT(getVkDevice().createPipelineLayout(&ci, ALLOCCB, &ret), "failed to create pipelineLayout");
      }
      return ret;
    };

    template<vk::SamplerCreateInfo CI> vk::Sampler getSampler() {
      scopeLock lock(&samplersMutex);
      static constexpr hash_t CIHASH = hash(CI);
      vk::Sampler& ret = samplers[CIHASH];
      if(!ret)
	VK_ASSERT(getVkDevice().createSampler(&CI, ALLOCCB, &ret), "failed to create sampler");
      return ret;
    };

  };

}
