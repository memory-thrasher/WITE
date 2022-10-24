#pragma once

#include <map>
#include <memory>
#include <atomic>

#include "Image.hpp"
#include "Queue.hpp"
#include "Vulkan.hpp"
#include "StructuralConstList.hpp"

namespace WITE::GPU {

  typedef uint64_t layer_t;
  typedef Collections::StructuralConstList<layer_t> layerCollection_t;

  class Gpu {
  private:
    struct LogicalGpu {
      std::map<vk::Format, vk::FormatProperties> formatProperties;
    };

    static std::atomic_bool inited;
    static vk::Instance vkInstance;
    static size_t gpuCount;
    static std::unique_ptr<Gpu[]> gpus;
    static uint64_t logicalPhysicalDeviceMatrix[64];//bitmask, which Gpu instances are included in each logical device
    static std::unique_ptr<struct LogicalGpu[]> logicalDevices;
    static size_t logicalDeviceCount;

    static uint64_t gpuMaskByLdm(uint64_t ldm);
    static constexpr vk::FormatFeatureFlags usageFeatures(uint64_t u) {
      vk::FormatFeatureFlags ret = (vk::FormatFeatureFlags)0;
      if(u & ImageBase::USAGE_VERTEX) ret |= vk::FormatFeatureFlagBits::eVertexBuffer;
      if(u & ImageBase::USAGE_DS_SAMPLED) {
	ret |= vk::FormatFeatureFlagBits::eSampledImage;
	if(u & ImageBase::USAGE_DS_WRITE) ret |= vk::FormatFeatureFlagBits::eStorageImage;
      }
      if(u & ImageBase::USAGE_ATT_DEPTH) ret |= vk::FormatFeatureFlagBits::eDepthStencilAttachment;
      if(u & ImageBase::USAGE_ATT_OUTPUT) ret |= vk::FormatFeatureFlagBits::eColorAttachment;
      if(u & ImageBase::USAGE_HOST_READ) ret |= vk::FormatFeatureFlagBits::eTransferSrc;
      if(u & ImageBase::USAGE_HOST_WRITE) ret |= vk::FormatFeatureFlagBits::eTransferDst;
      return ret;
    }

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

    Gpu(size_t idx, vk::PhysicalDevice);

  public:
    static void init(size_t logicalDeviceCount, const float* priorities);//TODO capabilities?
    static Gpu& get(size_t);
    static uint8_t gpuCountByLdm(uint64_t ldm);
    static inline bool ldmHasMultiplePhysical(uint64_t ldm) { return gpuCountByLdm(ldm) > 1; };
    static vk::Format getBestImageFormat(uint8_t comp, uint8_t compSize, uint64_t usage, uint64_t ldm = 1);

    size_t getIndex() { return idx; };
    vk::Device getVkDevice() { return dev; };//vkHandle is an opaque handle
  };

}
