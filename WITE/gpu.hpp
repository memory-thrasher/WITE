#pragma once

#include <map>
#include <memory>
#include <atomic>

#include "wite_vulkan.hpp"
#include "constants.hpp"

namespace WITE {

  class gpu {
  private:

    static std::atomic_bool inited;
    static vk::Instance vkInstance;
    static size_t gpuCount;
    static std::unique_ptr<gpu[]> gpus;
    static char appName[1024];

    size_t idx;
    std::map<vk::Format, vk::FormatProperties> formatProperties {};
    vk::PhysicalDevice pv;
    vk::PhysicalDeviceProperties2 pvp;
    typedef struct {
      vk::QueueFamilyProperties2 p;
      vk::QueueFamilyGlobalPriorityPropertiesKHR gpp;
    } qfp_t;
    std::unique_ptr<qfp_t[]> qfp;
    vk::Device dev;
    vk::Queue queue;
    vk::PipelineCache pipelineCache;
    vk::PhysicalDeviceMemoryProperties2 pdmp;
    vk::PhysicalDeviceMemoryBudgetPropertiesEXT pdmbp;
    std::array<int64_t, VK_MAX_MEMORY_TYPES> memoryScoreByType;
    std::array<vk::MemoryPropertyFlags, VK_MAX_MEMORY_TYPES> memoryFlagsByType;
    std::array<std::atomic_uint64_t, VK_MAX_MEMORY_HEAPS> freeMemoryByHeap;

    gpu(size_t idx, vk::PhysicalDevice);
    gpu(gpu&&) = delete;
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

    inline size_t getIndex() { return idx; };
    vk::Device getVkDevice() { return dev; };//vkHandle is an opaque handle
    vk::Queue getQueue();
    vk::PipelineCache getPipelineCache() { return pipelineCache; };
    inline auto getPhysical() { return pv; };
  };

}
