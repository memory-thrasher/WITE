#pragma once

#include <map>
#include <memory>
#include <atomic>

#include "Vulkan.hpp"

namespace WITE::GPU {

  class Gpu {
  private:
    static std::atomic_bool inited;
    static vk::Instance vkInstance;
    static std::unique_ptr<Gpu[]> gpus;
    static std::unique_ptr<struct LogicalDevice[]> ldevs;

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

    Gpu(size_t idx, vk::PhysicalDevice);

  public:
    static void init();

    Gpu& get(size_t);
  };

}
