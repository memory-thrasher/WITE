#pragma once

#include <vector>

#include "wite_vulkan.hpp"
#include "gpu.hpp"

namespace WITE {

  struct garbageCollector {
    size_t gpuIdx;
    std::vector<vk::Framebuffer> fbs;
    std::vector<vk::ImageView> imageViews;
    std::vector<vk::Image> images;
    std::vector<gpu::vram> rams;
    //add more as needed

    garbageCollector(size_t gpuIdx) : gpuIdx(gpuIdx) {};
    garbageCollector(const garbageCollector& o) = delete;
    garbageCollector(garbageCollector&& o) {
      gpuIdx = o.gpuIdx;
      fbs.swap(o.fbs);
      imageViews.swap(o.imageViews);
      images.swap(o.images);
      rams.swap(o.rams);
    };

    void collect() {
      gpu& dev = gpu::get(gpuIdx);
      // WARN("collecting for ", gpuIdx, " (", dev.getVkDevice(), ")");
      vk::Device vkdev = dev.getVkDevice();
      for(auto e : fbs)
	vkdev.destroy(e);
      fbs.clear();
      for(auto e : imageViews)
	vkdev.destroy(e);
      imageViews.clear();
      for(auto e : images)
	vkdev.destroy(e);
      images.clear();
      rams.clear();
      // WARN("end collecting for ", gpuIdx, " (", dev.getVkDevice(), ")");
    };

    ~garbageCollector() {
      collect();
    };

    inline void push(vk::Framebuffer e) {
      fbs.push_back(e);
    };

    inline void push(vk::ImageView e) {
      imageViews.push_back(e);
    };

    inline void push(vk::Image e) {
      images.push_back(e);
    };

    inline void push(gpu::vram& e) {
      rams.emplace_back(std::move(e));
    };

  };

}
