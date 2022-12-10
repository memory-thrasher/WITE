#pragma once

#include <map>
#include <memory>
#include <atomic>

#include "GpuResource.hpp"
#include "Queue.hpp"
#include "Vulkan.hpp"
#include "StructuralConstList.hpp"
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

    static deviceMask_t gpuMaskByLdm(deviceMask_t ldm);
    static constexpr vk::FormatFeatureFlags usageFeatures(usage_t u) {
      vk::FormatFeatureFlags ret = (vk::FormatFeatureFlags)0;
      if(u & GpuResource::USAGE_VERTEX) ret |= vk::FormatFeatureFlagBits::eVertexBuffer;
      if(u & GpuResource::USAGE_DS_SAMPLED) {
	ret |= vk::FormatFeatureFlagBits::eSampledImage;
	if(u & GpuResource::USAGE_DS_WRITE) ret |= vk::FormatFeatureFlagBits::eStorageImage;
      }
      if(u & GpuResource::USAGE_ATT_DEPTH) ret |= vk::FormatFeatureFlagBits::eDepthStencilAttachment;
      if(u & GpuResource::USAGE_ATT_OUTPUT) ret |= vk::FormatFeatureFlagBits::eColorAttachment;
      if(u & GpuResource::USAGE_HOST_READ) ret |= vk::FormatFeatureFlagBits::eTransferSrc;
      if(u & GpuResource::USAGE_HOST_WRITE) ret |= vk::FormatFeatureFlagBits::eTransferDst;
      return ret;
    };

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
    std::unique_ptr<class Queue[]> queues;
    Queue *graphics, *transfer, *compute;
    vk::PipelineCache pipelineCache;

    Gpu(size_t idx, vk::PhysicalDevice);

  public:
    static void init(size_t logicalDeviceCount, const float* priorities);//TODO capabilities?
    static void shutdown();
    static bool running;
    static Gpu& get(size_t);
    static inline size_t getGpuCount() { return gpuCount; };
    static uint8_t gpuCountByLdm(logicalDeviceMask_t ldm);
    static Gpu* getGpuFor(logicalDeviceMask_t ldm);//TODO distribute by weight, atomic seed over low modulo
    static inline bool ldmHasMultiplePhysical(logicalDeviceMask_t ldm) { return gpuCountByLdm(ldm) > 1; };
    static Collections::BitmaskIterator gpusForLdm(logicalDeviceMask_t ldm);
    static vk::Format getBestImageFormat(uint8_t comp, uint8_t compSize, usage_t usage, logicalDeviceMask_t ldm = 1);

    inline size_t getIndex() { return idx; };
    vk::Device getVkDevice() { return dev; };//vkHandle is an opaque handle
    Queue* getQueue(QueueType qt);
    inline Queue* getGraphics() { return graphics; };
    inline Queue* getCompute() { return compute; };
    inline Queue* getTransfer() { return transfer; };
    vk::PipelineCache getPipelineCache() { return pipelineCache; };
  };

}
