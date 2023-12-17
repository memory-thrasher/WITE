#pragma once

#include <map>
#include <memory>
#include <atomic>

#include "wite_vulkan.hpp"
#include "constants.hpp"
#include "stdExtensions.hpp"
#include "syncLock.hpp"
#include "DEBUG.hpp"

namespace WITE {

  class gpu {
  public:
    struct vram {
      vk::DeviceMemory handle;
      const vk::MemoryAllocateInfo mai;
      gpu* dev;
      vram() = default;//dummy for cpu-ram allocation
      vram(const vk::MemoryRequirements& mr, uint8_t type, gpu* dev);
      ~vram();
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
    vk::Queue queue;
    vk::PipelineCache pipelineCache;
    vk::PhysicalDeviceMemoryProperties2 pdmp;
    vk::PhysicalDeviceMemoryBudgetPropertiesEXT pdmbp;
    std::array<int64_t, VK_MAX_MEMORY_TYPES> memoryScoreByType;
    std::array<vk::MemoryPropertyFlags, VK_MAX_MEMORY_TYPES> memoryFlagsByType;
    std::array<std::atomic_uint64_t, VK_MAX_MEMORY_HEAPS> freeMemoryByHeap;
    std::map<hash_t, vk::Sampler> samplers;
    syncLock samplersMutex;

    gpu(size_t idx, vk::PhysicalDevice);
    gpu(gpu&&) = delete;
    void recordDeallocate(vram* doomed);
  public:
    static void init(const char* appName,
		     std::initializer_list<const char*> appRequestedLayers = {},
		     std::initializer_list<const char*> appRequestedExtensions = {});
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
    vk::PipelineCache getPipelineCache() { return pipelineCache; };
    inline auto getPhysical() { return pv; };
    void allocate(const vk::MemoryRequirements& mr, vk::MemoryPropertyFlags requiredFlags, vram* out);

    template<vk::SamplerCreateInfo CI> vk::Sampler getSampler() {
      scopeLock lock(&samplersMutex);
      vk::Sampler& ret = samplers[hash(vkToTuple(CI))];
      if(!ret) {
	VK_ASSERT(getVkDevice().createSampler(&CI, ALLOCCB, &ret), "failed to create sampler");
      }
      return ret;
    };

  };

}
