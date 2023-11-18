#pragma once

#include "wite_vulkan.hpp"
#include "gpu.hpp"
#include "templateStructs.hpp"

namespace WITE {

  struct imageBase {

    virtual ~imageBase() = default;

    virtual vk::Image frameImage(int64_t frame) const = 0;
    virtual vk::ImageMemoryBarrier2 memoryBarrier(size_t previousStep, int64_t frame) const = 0;

  };

  template<imageRequirements R>
  struct image : public imageBase {

    static_assert_show(isValid(R), R);

    static constexpr vk::ImageSubresourceRange allInclusiveSubresource {
      /* .aspectMask =*/ (R.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) ? vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eColor, //MAYBE multiplanar someday?
      /* .baseMipLevel =*/ 0,
      /* .levelCount =*/ R.mipLevels,
      /* .baseArrayLayer =*/ 0,
      /* .layerCount =*/ R.arrayLayers
    };

    static constexpr copyableArray<vk::ImageMemoryBarrier2, R.flowCount> defaultFlowTransitions = [](size_t i){
      const auto& src = R.flow[i],
	dst = R.flow[(i+1)%R.flowCount];
      vk::ImageMemoryBarrier2 ret;
      ret.setSrcStageMask(src.stages)
	.setSrcAccessMask(src.access)
	.setDstStageMask(dst.stages)
	.setDstAccessMask(dst.access)
	.setOldLayout(src.layout)
	.setNewLayout(dst.layout)
	.setSubresourceRange(allInclusiveSubresource);
      return ret;
    };

    copyableArray<vk::ImageMemoryBarrier2, R.flowCount*R.frameswapCount> flowTransitions;//i = frameswapIdx * steps + oldStepIdx

    vk::Image vkImage[R.frameswapCount];
    vk::MemoryRequirements mrs[R.frameswapCount];
    gpu::vram rams[R.frameswapCount];
    vk::ImageCreateInfo ci;

    image(vk::Extent2D ext) : image(ext.width, ext.height) {};

    image(uint32_t w, uint32_t h, uint32_t d = 1) {
      ci = getDefaultCI(R);
      ci.extent.width = w;
      ci.extent.height = h;
      ci.extent.depth = d;
      gpu& dev = gpu::get(R.deviceId);
      vk::MemoryPropertyFlags flags = (R.imageFlags & imageFlags_e::eHostVisible) ?
	vk::MemoryPropertyFlagBits::eHostCoherent :
	vk::MemoryPropertyFlagBits::eDeviceLocal;
      for(size_t i = 0;i < R.frameswapCount;i++) {
	VK_ASSERT(gpu::get(R.deviceId).getVkDevice().createImage(&ci, ALLOCCB, &vkImage[i]), "failed to allocate image");
	dev.getVkDevice().getImageMemoryRequirements(vkImage[i], &mrs[i]);
	dev.allocate(mrs[i], flags, &rams[i]);
      }
      for(size_t i = 0;i < R.flowCount;i++)
	for(size_t j = 0;j < R.frameswapCount;i++)
	  (flowTransitions[i*R.flowCount+j] = defaultFlowTransitions[i]).setImage(vkImage[j]);
    };

    virtual vk::Image frameImage(int64_t frame) const override {
      ASSERT_TRAP(frame >= -R.frameswapCount, "requested frame too far before frame 0");
      if(frame < 0) [[unlikely]] frame += R.frameswapCount;
      return vkImage[frame % R.frameswapCount];
    };

    virtual vk::ImageMemoryBarrier2 memoryBarrier(size_t previousStep, int64_t frame) const override {
      ASSERT_TRAP(frame >= -R.frameswapCount, "requested frame too far before frame 0");
      if(frame < 0) [[unlikely]] frame += R.frameswapCount;
      return flowTransitions[(frame % R.frameswapCount) * R.flowCount + previousStep];
    };

  };

};

