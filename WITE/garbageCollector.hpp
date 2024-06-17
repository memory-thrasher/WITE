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
