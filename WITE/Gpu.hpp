#pragma once

#include <map>
#include <memory>
#include <atomic>

#include "GpuResource.hpp"
#include "Queue.hpp"
#include "Vulkan.hpp"
#include "types.hpp"
#include "constants.hpp"
#include "BitmaskIterator.hpp"

namespace WITE::GPU {

  class Gpu {
  private:
    struct LogicalGpu {
      std::map<vk::Format, vk::FormatProperties> formatProperties;
    };

    static std::atomic_bool inited;
    static vk::Instance vkInstance;
    static size_t gpuCount;
    static std::unique_ptr<Gpu[]> gpus;
    static deviceMask_t logicalPhysicalDeviceMatrix[MAX_LDMS];//bitmask, which Gpu instances are included in each logical device
    static std::unique_ptr<struct LogicalGpu[]> logicalDevices;
    static size_t logicalDeviceCount;
    static char appName[1024];

    static deviceMask_t gpuMaskByLdm(deviceMask_t ldm);

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
    std::unique_ptr<class Queue[]> queues;
    Queue *graphics, *transfer, *compute;
    vk::PipelineCache pipelineCache;
    vk::PhysicalDeviceMemoryProperties2 pdmp;
    vk::PhysicalDeviceMemoryBudgetPropertiesEXT pdmbp;
    std::array<int64_t, VK_MAX_MEMORY_TYPES> memoryScoreByType;
    std::array<vk::MemoryPropertyFlags, VK_MAX_MEMORY_TYPES> memoryFlagsByType;
    std::array<std::atomic_uint64_t, VK_MAX_MEMORY_HEAPS> freeMemoryByHeap;

    Gpu(size_t idx, vk::PhysicalDevice);
    Gpu(Gpu&&) = delete;
  public:
    static void init(size_t logicalDeviceCount, const float* priorities, const char* appName,
		     std::initializer_list<const char*> appRequestedLayers,
		     std::initializer_list<const char*> appRequestedExtensions);
    static void shutdown();
    static bool running;
    static Gpu& get(size_t);
    static inline size_t getGpuCount() { return gpuCount; };
    static uint8_t gpuCountByLdm(logicalDeviceMask_t ldm);
    static Gpu& getGpuFor(logicalDeviceMask_t ldm);
    static inline bool ldmHasMultiplePhysical(logicalDeviceMask_t ldm) { return gpuCountByLdm(ldm) > 1; };
    static inline bool ldmContains(logicalDeviceMask_t ldm, size_t gpu) { return gpuMaskByLdm(ldm) & (1 << gpu); };
    static inline Collections::BitmaskIterator gpusForLdm(logicalDeviceMask_t ldm) { return { gpuMaskByLdm(ldm) }; };
    static vk::Format getBestImageFormat(uint8_t comp, uint8_t compSize, usage_t usage, logicalDeviceMask_t ldm, bool linear);
    static inline const char* getAppName() { return appName; };
    static inline auto getVkInstance() { return vkInstance; };

    inline size_t getIndex() { return idx; };
    vk::Device getVkDevice() { return dev; };//vkHandle is an opaque handle
    Queue* getQueue(QueueType qt);
    inline Queue* getGraphics() { return graphics; };
    inline Queue* getCompute() { return compute; };
    inline Queue* getTransfer() { return transfer; };
    inline bool canGraphicsCompute() { return getGraphics()->supportsCompute(); };
    vk::PipelineCache getPipelineCache() { return pipelineCache; };
    void allocate(const vk::MemoryRequirements&, vk::MemoryPropertyFlags requiredFlags, VRam* out);
    void deallocate(VRam*);
    inline auto getPhysical() { return pv; };
  };

}
