#pragma once

#include "wite_vulkan.hpp"
#include "gpu.hpp"
#include "templateStructs.hpp"

namespace WITE {

  template<imageRequirements R> struct image {

    static_assert_show(isValid(R), R);

    vk::Image vkImage[R.frameswapCount];
    vk::MemoryRequirements mrs[R.frameswapCount];
    gpu::vram rams[R.frameswapCount];
    vk::ImageCreateInfo ci;

    image(vk::Extent2D ext) : image(ext.width, ext.height) {};

    image(uint32_t w, uint32_t h, uint32_t d = 1) {
      ci = getDefaultCI(R);
      ci.extent.width = w;
      ci.extent.height = R.dimensions > 1 ? h : 1;
      ci.extent.depth = R.dimensions > 2 ? d : 1;
      gpu& dev = gpu::get(R.deviceId);
      vk::MemoryPropertyFlags flags;
      if constexpr(R.hostVisible) {
	flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
      } else {
	flags = vk::MemoryPropertyFlagBits::eHostCoherent;
      }
      for(size_t i = 0;i < R.frameswapCount;i++) {
	VK_ASSERT(gpu::get(R.deviceId).getVkDevice().createImage(&ci, ALLOCCB, &vkImage[i]), "failed to allocate image");
	dev.getVkDevice().getImageMemoryRequirements(vkImage[i], &mrs[i]);
	dev.allocate(mrs[i], flags, &rams[i]);
      }
    };

    image() : image(256, 256) {}

    //TODO resize

    vk::Image frameImage(int64_t frame) const {
      ASSERT_TRAP(frame >= -R.frameswapCount, "requested frame too far before frame 0");
      if(frame < 0) [[unlikely]] frame += R.frameswapCount;
      return vkImage[frame % R.frameswapCount];
    };

  };

};

