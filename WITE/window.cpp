/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "window.hpp"
#include "SDL.hpp"
#include "gpu.hpp"
#include "profiler.hpp"
#include "configuration.hpp"

namespace WITE {

  inline intBox3D intBoxFromSdlRect(SDL_Rect& rect) {
    return intBox3D(rect.x, rect.x + rect.w, rect.y, rect.y + rect.h);
  };

  inline intBox3D window::getScreenBounds(size_t idx) {//static
    SDL_Rect rect;
    SDL_GetDisplayUsableBounds(idx, &rect);
    return intBoxFromSdlRect(rect);
  };

  intBox3D getDefaultSize() {//static
    PROFILEME;
    const char* cliExtentRaw = configuration::getOption("extent=");
    if(cliExtentRaw) {
      char cliExtentStorage[64];//because strtoul offset output needs a mutable char*
      char* cliExtent = cliExtentStorage;
      ASSERT_TRAP(std::strlen(cliExtentRaw) < 64, "extent cli option has too many characters");
      WITE::strcpy(cliExtent, cliExtentRaw, 64);
      LOG("using requested extent ", cliExtent);
      unsigned long x = std::strtoul(cliExtent, &cliExtent, 10);
      unsigned long y = std::strtoul(cliExtent + 1, &cliExtent, 10);
      unsigned long w = std::strtoul(cliExtent + 1, &cliExtent, 10);
      unsigned long h = std::strtoul(cliExtent + 1, &cliExtent, 10);
      return intBox3D(x, w+x, y, h+y);
    } else {
      LOG("no extent requested, using default");
      const char* cliScreen = configuration::getOption("screen");
      auto wholeScreen = window::getScreenBounds(cliScreen ? std::strtoul(cliScreen, NULL, 10) : 0);
      auto center = wholeScreen.center();
      uint64_t w = max(192, wholeScreen.width()/4), h = w * 108 / 192;
      return intBox3D(center.x - w/2, center.x + w/2, center.y - h/2, center.y + h/2);
    }
  };

  vk::PresentModeKHR window::getPreferredPresentMode() {//static
    //mailbox is the default preferred mode. User may pick another via cli argument.
    //If what is picked is not supported, fifo will be used.
    const char* v = configuration::getOption("presentmode");
    if(v == NULL || strcmp(v, "mailbox") == 0)
      return vk::PresentModeKHR::eMailbox;//no tare, uncapped
    // if(strcmp(v, "sharedcontinuousrefresh") == 0)
    //   return vk::PresentModeKHR::eSharedContinuousRefresh;
    // if(strcmp(v, "shareddemandrefresh") == 0)
    //   return vk::PresentModeKHR::eSharedDemandRefresh;//nyi, different interface
    if(strcmp(v, "fiforelaxed") == 0)
      return vk::PresentModeKHR::eFifoRelaxed;//screen tare possible, only when under framerate, capped otherwise
    if(strcmp(v, "fifo") == 0)
      return vk::PresentModeKHR::eFifo;//framerate capped to monitor (vsync)
    if(strcmp(v, "immediate") == 0)
      return vk::PresentModeKHR::eImmediate;//screen tare likely
    WARN("Requested present mode not recognized, using fifo (vsync)");
    return vk::PresentModeKHR::eFifo;
  };

  window::window(size_t gpuIdx) : window(gpuIdx, getDefaultSize()) {};

  window::window(size_t gpuIdx, intBox3D box) : gpuIdx(gpuIdx) {
    PROFILEME;
    //a rendered image is always blitted to the window's swapchain image
    x = (uint32_t)box.minx;
    y = (uint32_t)box.miny;
    w = (uint32_t)box.width();
    h = (uint32_t)box.height();
    WARN("Creating window with extent: ", x, ",", y, " ", w, "x", h);
    sdlWindow = SDL_CreateWindow("WITE", x, y, w, h, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS);
    gpu& g = gpu::get(gpuIdx);
    auto phys = g.getPhysical();
    VkSurfaceKHR vksurface;
    ASSERT_TRAP_OR_RUN(SDL_Vulkan_CreateSurface(sdlWindow, gpu::getVkInstance(), &vksurface), "Failed to create vk surface");
    surface = vksurface;
    uint32_t presentModeCount = 10;
    vk::PresentModeKHR presentModes[10];//skipping a call to save a malloc, there are only 5 present modes in existance atm
    VK_ASSERT(phys.getSurfacePresentModesKHR(surface, &presentModeCount, presentModes), "failed to enumerate present modes");
    vk::PresentModeKHR preferredMode = getPreferredPresentMode();
    vk::PresentModeKHR mode = vk::PresentModeKHR::eFifo;
    if(contains(presentModes, preferredMode))
      mode = preferredMode;
    else
      WARN("Requested present mode not supported, using fifo (vsync always on).");
    swapCI.setMinImageCount(3).setImageArrayLayers(1).setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear)
      .setImageUsage(vk::ImageUsageFlagBits::eTransferDst).setImageSharingMode(vk::SharingMode::eExclusive)
      .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity).setPresentMode(mode)
      .setClipped(true).setSurface(surface);
    //now that the window exists, we can ask each device if it can render to it
    vk::Bool32 supported = false;
    VK_ASSERT(phys.getSurfaceSupportKHR(g.getQueueFam(), surface, &supported), "failed to query surface support");
    ASSERT_TRAP(supported, "Device cannot render to this window!");
    uint32_t formatCount;
    {
      VK_ASSERT(phys.getSurfaceFormatsKHR(surface, &formatCount, NULL), "Could not count surface formats");
      auto formats = std::make_unique<vk::SurfaceFormatKHR[]>(formatCount);
      VK_ASSERT(phys.getSurfaceFormatsKHR(surface, &formatCount, formats.get()), "Could not enumerate surface formats");
      swapCI.imageFormat = formats[0].format;
      //TODO match integer-ness with presentable image because blit can't cross int/uint/float
      //file:///home/sid/Downloads/Vulkan%201.3.html#VUID-vkCmdBlitImage-srcImage-00230
      if(swapCI.imageFormat == vk::Format::eUndefined)//vk standard meaning "any is fine"
	swapCI.imageFormat = vk::Format::eB8G8R8Unorm;
      else
	swapCI.setImageColorSpace(formats[0].colorSpace);
    }
    VK_ASSERT(phys.getSurfaceCapabilitiesKHR(surface, &surfCaps), "Could not fetch surface capabilities.");
    swapCI.setImageExtent({
	clamp(w, surfCaps.minImageExtent.width,  surfCaps.maxImageExtent.width),
	clamp(h, surfCaps.minImageExtent.height, surfCaps.maxImageExtent.height)
      });
    swapCI.setCompositeAlpha(static_cast<vk::CompositeAlphaFlagBitsKHR>(1 << (WITE::ffs(static_cast<vk::CompositeAlphaFlagsKHR::MaskType>(surfCaps.supportedCompositeAlpha)) - 1)));
    auto dev = g.getVkDevice();
    VK_ASSERT(dev.createSwapchainKHR(&swapCI, ALLOCCB, &swap), "could not create swapchain");
    VK_ASSERT(dev.getSwapchainImagesKHR(swap, &swapImageCount, NULL), "Could not count swapchain images.");
    swapImages = std::make_unique<vk::Image[]>(swapImageCount);
    VK_ASSERT(dev.getSwapchainImagesKHR(swap, &swapImageCount, swapImages.get()), "Could not get swapchain images");
#ifdef WITE_DEBUG_IMAGE_BARRIERS
    for(size_t i = 0;i < swapImageCount;i++)
      WARN("Fetched image with handle ", std::hex, swapImages[i], std::dec, " from swapchain for window");
#endif
    swapSemCount = swapImageCount + 2;
    acquisitionSems = std::make_unique<vk::Semaphore[]>(swapSemCount);
    blitSems = std::make_unique<vk::Semaphore[]>(swapSemCount);
    vk::SemaphoreCreateInfo semCI;//intentionally default constructed.
    cmdFences = std::make_unique<vk::Fence[]>(swapSemCount);
    vk::FenceCreateInfo fenceCI(vk::FenceCreateFlagBits::eSignaled);//default
    for(size_t i = 0;i < swapSemCount;i++) {
      VK_ASSERT(dev.createSemaphore(&semCI, ALLOCCB, &acquisitionSems[i]), "failed to create semaphore");
      VK_ASSERT(dev.createSemaphore(&semCI, ALLOCCB, &blitSems[i]), "failed to create semaphore");
      VK_ASSERT(dev.createFence(&fenceCI, ALLOCCB, &cmdFences[i]), "failed to create fence");
    }
    const vk::CommandPoolCreateInfo poolCI(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, g.getQueueFam());
    VK_ASSERT(dev.createCommandPool(&poolCI, ALLOCCB, &cmdPool), "failed to allocate command pool");
    vk::CommandBufferAllocateInfo allocInfo(cmdPool, vk::CommandBufferLevel::ePrimary, swapSemCount);
    cmds = std::make_unique<vk::CommandBuffer[]>(swapSemCount);
    VK_ASSERT(dev.allocateCommandBuffers(&allocInfo, cmds.get()), "failed to allocate command buffer");
  };

  window::~window() {
    SDL_DestroyWindow(sdlWindow);
    auto vkdev = gpu::get(gpuIdx).getVkDevice();
    vkdev.destroy(cmdPool);
    for(size_t i = 0;i < swapSemCount;i++) {
      vkdev.destroy(acquisitionSems[i], ALLOCCB);
      vkdev.destroy(blitSems[i], ALLOCCB);
      vkdev.destroy(cmdFences[i], ALLOCCB);
    }
    vkdev.destroy(swap, ALLOCCB);
  };

  void window::addInstanceExtensionsTo(std::vector<const char*>& extensions) {//static
    PROFILEME;
    auto tempWin = SDL_CreateWindow("surface probe", 0, 0, 100, 100, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    unsigned int cnt = 0;
    ASSERT_TRAP_OR_RUN(SDL_Vulkan_GetInstanceExtensions(tempWin, &cnt, NULL), "failed to count vk extensions required by sdl");
    const char**names = new const char*[cnt];
    ASSERT_TRAP_OR_RUN(SDL_Vulkan_GetInstanceExtensions(tempWin, &cnt, names), "failed to list vk extensions required by sdl");
    SDL_DestroyWindow(tempWin);
    extensions.reserve(extensions.size() + cnt);
    for(size_t i = 0;i < cnt;i++)
      extensions.push_back(names[i]);
    delete [] names;
  };

  glm::uvec2 window::getUvecSize() {
    int w, h;
    SDL_Vulkan_GetDrawableSize(sdlWindow, &w, &h);
    return { (uint32_t)w, (uint32_t)h };
  };

  glm::vec2 window::getVecSize() {
    int w, h;
    SDL_Vulkan_GetDrawableSize(sdlWindow, &w, &h);
    return { w, h };
  };

  vk::Extent2D window::getSize() {
    int w, h;
    SDL_Vulkan_GetDrawableSize(sdlWindow, &w, &h);
    return { (uint32_t)w, (uint32_t)h };
  };

  vk::Extent3D window::getSize3D() {//because iamges are 3d, easier to accomodate here than convert in caller
    int w, h;
    SDL_Vulkan_GetDrawableSize(sdlWindow, &w, &h);
    return { (uint32_t)w, (uint32_t)h, 1 };
  };

  void window::present(vk::Image src, vk::ImageLayout srcLayout, vk::Offset3D srcSize, vk::SemaphoreSubmitInfo& renderWaitSem) {
    PROFILEME;
    ASSERT_TRAP(srcSize.z == 1, "attempted to present 3d image");
    auto& dev = gpu::get(gpuIdx);
    auto vkdev = dev.getVkDevice();
    uint32_t presentImageIndex;
    {
      PROFILEME_MSG("acquire");
      auto acquireRes = vkdev.acquireNextImageKHR(swap, 0, acquisitionSems[activeSwapSem], VK_NULL_HANDLE, &presentImageIndex);
      switch(acquireRes) {
      case vk::Result::eTimeout:
      case vk::Result::eNotReady:
	std::atomic_fetch_add_explicit(&framesSkipped, 1, std::memory_order_relaxed);
	return;
      case vk::Result::eSuboptimalKHR:
	//TODO handle
      case vk::Result::eSuccess:
	//proceed with present
	break;
      default:
	VK_ASSERT(acquireRes, "failed to acquire");
	break;
      }
    }
    auto dst = swapImages[presentImageIndex];
    vk::ImageMemoryBarrier2 barrier1 {
      vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
      vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
      vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
      {}, {}, dst, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    }, barrier2 {
      vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
      vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
      vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR,
      {}, {}, dst, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    };
    vk::DependencyInfo di { {}, 0, NULL, 0, NULL, 1, &barrier1 };
    vk::CommandBuffer cmd = cmds[activeSwapSem];
#ifdef WITE_DEBUG_FENCES
    WARN("window using cmd ", cmd, " and window fence ", activeSwapSem);
#endif
    {
      PROFILEME_MSG("Waiting for fence");
      VK_ASSERT(vkdev.waitForFences(1, &cmdFences[activeSwapSem], false, 10000000000 /* 10 seconds */),
		"Present fence timeout");
    }
#ifdef WITE_DEBUG_FENCES
    WARN("window wait complete for ", activeSwapSem);
#endif
    VK_ASSERT(vkdev.resetFences(1, &cmdFences[activeSwapSem]), "Failed to reset fence (?how?)");
    {
      PROFILEME_MSG("recording vkcmd");
      vk::CommandBufferBeginInfo begin { vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
      VK_ASSERT(cmd.begin(&begin), "failed to begin a command buffer");
      WITE_DEBUG_IB(barrier1, cmd);
      cmd.pipelineBarrier2(&di);
      // vk::ClearColorValue color { 1.0f, 1.0f, 1.0f, 1.0f };
      // vk::ImageSubresourceRange range { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
      // cmd.clearColorImage(dst, srcLayout, &color, 1, &range);
      vk::ImageBlit2 region {
	{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, {{{0, 0, 0}, srcSize}},
	{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, {{{0, 0, 0}, { (int32_t)w, (int32_t)h, 1 }}}
      };
      //must blit if only for format conversion, even if they are the same size (we can't know the surface format at compile time)
      vk::BlitImageInfo2 blit { src, srcLayout, dst, vk::ImageLayout::eTransferDstOptimal, 1, &region, vk::Filter::eNearest };
      cmd.blitImage2(&blit);
      di.pImageMemoryBarriers = &barrier2;
      cmd.pipelineBarrier2(&di);
      WITE_DEBUG_IB(barrier2, cmd);
      VK_ASSERT(cmd.end(), "failed to end a command buffer");
    }
    vk::SemaphoreSubmitInfo waits[2] = {
      renderWaitSem,
      { acquisitionSems[activeSwapSem], 0, vk::PipelineStageFlagBits2::eNone }
    }, signal { blitSems[activeSwapSem], 0, vk::PipelineStageFlagBits2::eNone };
    vk::CommandBufferSubmitInfo cmdSubmitInfo(cmd);
    vk::SubmitInfo2 submit({}, 2, waits, 1, &cmdSubmitInfo, 1, &signal);
      vk::PresentInfoKHR presentInfo { 1, &blitSems[activeSwapSem], 1, &swap, &presentImageIndex, NULL };
    {
      PROFILEME_MSG("submitting vkcmd");
      scopeLock lock(dev.getQueueMutex());
      VK_ASSERT(dev.getQueue().submit2(1, &submit, cmdFences[activeSwapSem]), "failed to submit command buffer");
    }
    {
      PROFILEME_MSG("present");
      VK_ASSERT(dev.getQueue().presentKHR(&presentInfo), "Present failed");
    }
    activeSwapSem++;
    if(activeSwapSem >= swapSemCount) [[unlikely]] activeSwapSem = 0;
    //TODO handle suboptimal return. Send resize request to target_t?
  };

  void window::hide() {
    SDL_HideWindow(sdlWindow);
  };

  void window::show() {
    SDL_ShowWindow(sdlWindow);
  };


}
