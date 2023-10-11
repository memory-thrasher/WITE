#pragma once

#include "ExportResource.hpp"
#include "Gpu.hpp"

namespace WITE {

  struct imageBase {
    vk::Image vkImage;
    vk::ImageCreateInfo ci;

    virtual ~imageBase() = default;

  };

  template<imageResourceRequirements R>
  struct image : public imageBase {

    image(uint32_t w, uint32_t h, uint32_t d = 1) {
      ci = R.getDefaultCI();
      ci.extent.width = w;
      ci.extent.height = y;
      ci.extent.depth = d;
      VKASSERT(Gpu::get(R.deviceId).createImage(&ci, VKALLOC, &vkImage));
    };

  };

};

