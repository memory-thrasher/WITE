#pragma once

#include "ExportResource.hpp"
#include "Gpu.hpp"

namespace WITE {

  struct imageBase {

    virtual ~imageBase() = default;

    virtual vk::Image frameImage(uint64_t frame) const = 0;

  };

  template<imageRequirements R>
  struct image : public imageBase {

    static constexpr vk::ImageSubresourceRange allInclusiveSubresource {
      .aspectMask = (R.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) ?
      vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eColor,
      //MAYBE multiplanar someday? ^^
      .baseMipLevel = 0,
      .levelCount = R.mipLevels,
      .baseArrayLayer = 0,
      .layerCount = R.arrayLayers
    };

    static constexpr CopyableArray<vk::ImageMemoryBarrier2, R.flow.len> defaultFlowTransitions = [](size_t i){
      auto& src = R[i],
	dst = R[(i+1)%R.len];
      return {
	.srcStageMask = src.stages,
	.srcAccessMask = src.access,
	.dstStageMask = dst.stages,
	.dstAccessMask = dst.access,
	.oldLayout = src.layout,
	.newLayout = dst.layout,
	.subresourceRange = allInclusiveSubresource
      };
    };

    const CopyableArray<vk::ImageMemoryBarrier2, R.flow.len> flowTransitions = [](size_t i){
      vk::ImageMemoryBarrier2 ret = defaultFlowTransitions[i];
      ret.setImage(vkImage);
      return ret;
    };

    vk::Image vkImage[R.frameswapCount];
    vk::ImageCreateInfo ci;

    image(uint32_t w, uint32_t h, uint32_t d = 1) {
      ci = getDefaultCI(R);
      ci.extent.width = w;
      ci.extent.height = y;
      ci.extent.depth = d;
      for(size_t i = 0;i < R.frameswapCount;i++)
	VKASSERT(Gpu::get(R.deviceId).createImage(&ci, VKALLOC, &vkImage[i]));
    };

    virtual vk::Image frameImage(int64_t frame) const {
      if(frame < 0) [[unlikely]] frame += R.frameswapCount;
      return vkImage[frame % R.frameswapCount];
    };

  };

};

