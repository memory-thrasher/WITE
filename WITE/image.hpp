/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include "wite_vulkan.hpp"
#include "gpu.hpp"
#include "onionTemplateStructs.hpp"
#include "garbageCollector.hpp"
#include "DEBUG.hpp"

namespace WITE {

  template<imageRequirements R> struct image {

    static_assert_show(isValid(R), R);

    vk::Image vkImage[R.frameswapCount];
    vk::MemoryRequirements mrs[R.frameswapCount];
    gpu::vram rams[R.frameswapCount];
    vk::ImageCreateInfo ci;
    vk::MemoryPropertyFlags flags;
    //dirty check: track when the image create info (size) was late updated, and when each image was created.
    uint64_t frameCiUpdated = 0, frameImageCreated[R.frameswapCount];
    vk::Extent3D frameImageExtent[R.frameswapCount];

    image(vk::Extent2D ext) : image(ext.width, ext.height) {};

    image(uint32_t w, uint32_t h, uint32_t d = 1) {
      static constexpr vk::ImageCreateInfo defaultCI = getDefaultCI(R);
      ci = defaultCI;
      ci.extent.width = w;
      ci.extent.height = R.dimensions > 1 ? h : 1;
      ci.extent.depth = R.dimensions > 2 ? d : 1;
      gpu& dev = gpu::get(R.deviceId);
      if constexpr(R.hostVisible) {
	flags = vk::MemoryPropertyFlagBits::eHostCoherent;
      } else {
	flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
      }
      for(size_t i = 0;i < R.frameswapCount;i++) {
	VK_ASSERT(gpu::get(R.deviceId).getVkDevice().createImage(&ci, ALLOCCB, &vkImage[i]), "failed to allocate image");
#ifdef WITE_DEBUG_IMAGE_BARRIERS
	WARN("Created image with handle ", std::hex, vkImage[i], std::dec, " for requirement ", R.id);
#endif
	dev.getVkDevice().getImageMemoryRequirements(vkImage[i], &mrs[i]);
	dev.allocate(mrs[i], flags, &rams[i]);
	VK_ASSERT(dev.getVkDevice().bindImageMemory(vkImage[i], rams[i].handle, 0), "failed to bind image memory");
	frameImageExtent[i] = ci.extent;
	frameImageCreated[i] = 0;
	ASSERT_TRAP(frameImageCreated[i] == 0, "assignment fail");//yes, this has triggered. ASM for assignment above seemed missing. Compiler bug?
      }
    };

    image() : image(256, 256) {}

    ~image() {
      gpu& dev = gpu::get(R.deviceId);
      for(size_t i = 0;i < R.frameswapCount;i++) {
	dev.getVkDevice().destroyImage(vkImage[i], ALLOCCB);
	rams[i].free();
      }
    };

    inline size_t frameImageIdx(uint64_t frame) const {
      if(frame < 0) [[unlikely]] frame += R.frameswapCount;
      return frame % R.frameswapCount;
    }

    inline uint64_t frameUpdated(uint64_t currentFrame) {
      return frameImageCreated[frameImageIdx(currentFrame)];
    }

    void resize(uint64_t frame, vk::Extent3D& size) {
      ASSERT_TRAP(size.depth == 1 || R.dimensions > 2, "invalid size");
      ASSERT_TRAP(size.height == 1 || R.dimensions > 1, "invalid size");
      ci.extent = size;
      frameCiUpdated = frame;
    };

    template<resizeBehavior_t B, vk::ImageLayout layout>
    inline void applyPendingResize(uint64_t frame, vk::CommandBuffer cmd, garbageCollector& gc) {
      size_t idx = frameImageIdx(frame);
      ASSERT_TRAP(frame >= frameImageCreated[idx], "image creation framestamp is in the future");
      if(frameCiUpdated <= frameImageCreated[idx])
	return;
      if constexpr(B.image.type == imageResizeType::eNone) {
	WARN("Resize is pending but ignored because resize behavior is None. frame size updated: ", frameCiUpdated, "; frame image created: ", frameImageCreated[idx]);
	return;
      } else {
	gpu& dev = gpu::get(R.deviceId);
	frameImageCreated[idx] = frame;
	vk::Image oldImage = vkImage[idx];
	gpu::vram oldRam = std::move(rams[idx]);
	auto oldExtent = frameImageExtent[idx];
	frameImageExtent[idx] = ci.extent;
	VK_ASSERT(gpu::get(R.deviceId).getVkDevice().createImage(&ci, ALLOCCB, &vkImage[idx]), "failed to allocate image");
	dev.getVkDevice().getImageMemoryRequirements(vkImage[idx], &mrs[idx]);
	dev.allocate(mrs[idx], flags, &rams[idx]);
	VK_ASSERT(dev.getVkDevice().bindImageMemory(vkImage[idx], rams[idx].handle, 0), "failed to bind image memory");
	static constexpr vk::ImageMemoryBarrier2 oldToSrc = getBarrierSimple<R>(layout, vk::ImageLayout::eTransferSrcOptimal),
	  newToDst = getBarrierSimple<R>(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal),
	  newToOut = getBarrierSimple<R>(vk::ImageLayout::eTransferDstOptimal, layout);
	vk::ImageMemoryBarrier2 phase2 = newToOut;
	vk::DependencyInfo di;
	static constexpr vk::ImageSubresourceRange SR = getAllInclusiveSubresource(R);
	if constexpr(B.image.type == imageResizeType::eBlit || B.image.type == imageResizeType::eClear) {
	  static_assert(R.usage & vk::ImageUsageFlagBits::eTransferDst);//need transfer dst to blit or clear
	  static constexpr uint32_t phase1Count = B.image.type == imageResizeType::eBlit ? 2 : 1;
	  vk::ImageMemoryBarrier2 phase1[phase1Count] = { newToDst, oldToSrc };
	  phase1[0].image = vkImage[idx];
	  if constexpr(phase1Count > 1)
	    phase1[1].image = oldImage;
	  vk::DependencyInfo di;
	  di.pImageMemoryBarriers = phase1;
	  di.imageMemoryBarrierCount = phase1Count;
	  cmd.pipelineBarrier2(&di);
	  if constexpr(B.image.type == imageResizeType::eBlit) {
	    static_assert(R.usage & vk::ImageUsageFlagBits::eTransferSrc);//need transfer src to blit
	    vk::ImageBlit region { SR, { { 0, 0 }, oldExtent }, SR, { { 0, 0 }, ci.extent } };
	    cmd.blitImage(oldImage, vk::ImageLayout::eTransferSrcOptimal, vkImage[idx], vk::ImageLayout::eTransferDstOptimal, 1, &region, vk::Filter::eLinear);
	  } else if constexpr(B.image.type == imageResizeType::eClear) {
	    if constexpr(R.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) {
	      cmd.clearDepthStencilImage(vkImage[idx], vk::ImageLayout::eTransferDstOptimal, B.image.clearValue.depthStencil, 1, &SR);
	    } else {
	      cmd.clearColorImage(vkImage[idx], vk::ImageLayout::eTransferDstOptimal, B.image.clearValue.color, 1, &SR);
	    }
	  }
	} else {
	  phase2.oldLayout = vk::ImageLayout::eUndefined;
	}
	phase2.image = vkImage[idx];
	di.pImageMemoryBarriers = &phase2;
	di.imageMemoryBarrierCount = 1;
	cmd.pipelineBarrier2(&di);
	gc.push(oldImage);
	gc.push(oldRam);
#ifdef WITE_DEBUG_IMAGE_BARRIERS
	WARN("Created image with handle ", std::hex, vkImage[idx], " to replace image ", oldImage, std::dec, " as part of resizing operation to size ", ci.extent.width, ", ", ci.extent.height, ", ", ci.extent.depth, ". New image has been transitioned to layout ", (int)phase2.newLayout);
#endif
      }
    };

    inline vk::Extent3D getSizeExtent(uint64_t frame) const {
      return frameImageExtent[frameImageIdx(frame)];
    };

    inline vk::Offset3D getSizeOffset(uint64_t frame) const {
      auto extent = frameImageExtent[frameImageIdx(frame)];
      return { (int32_t)extent.width, (int32_t)extent.height, (int32_t)extent.depth };
    };

    inline vk::Rect2D getSizeRect(uint64_t frame) const {
      auto extent = frameImageExtent[frameImageIdx(frame)];
      return { {0, 0}, {extent.width, extent.height} };
    };

    vk::Image frameImage(uint64_t frame) const {
      return vkImage[frameImageIdx(frame)];
    };

    template<vk::ImageViewType VT, vk::ImageSubresourceRange SR> vk::ImageView createView(uint64_t frame) const {
      vk::ImageViewCreateInfo ci { {}, {}, VT, R.format, {}, SR };
      ci.image = frameImage(frame);
      vk::ImageView ret;
      VK_ASSERT(gpu::get(R.deviceId).getVkDevice().createImageView(&ci, ALLOCCB, &ret), "failed to create image view");
      return ret;
    };

  };

};

