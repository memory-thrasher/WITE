#include "window.hpp"
#include "SDL.hpp"
#include "gpu.hpp"
#include "profiler.hpp"

namespace WITE {

  inline intBox3D intBoxFromSdlRect(SDL_Rect& rect) {
    return intBox3D(rect.x, rect.x + rect.w, rect.y, rect.y + rect.h);
  };

  inline intBox3D window::getScreenBounds(size_t idx) {//static
    SDL_Rect rect;
    SDL_GetDisplayUsableBounds(idx, &rect);
    return intBoxFromSdlRect(rect);
  };

  intBox3D getDefaultSize() {
    PROFILEME;
    char* cliExtent = gpu::getOption("extent");
    if(cliExtent) {
      unsigned long x = std::strtoul(cliExtent, &cliExtent, 10);
      unsigned long y = std::strtoul(cliExtent + 1, &cliExtent, 10);
      unsigned long w = std::strtoul(cliExtent + 1, &cliExtent, 10);
      unsigned long h = std::strtoul(cliExtent + 1, &cliExtent, 10);
      return intBox3D(x, w+x, y, h+y);
    } else {
      char* cliScreen = gpu::getOption("screen");
      auto wholeScreen = window::getScreenBounds(cliScreen ? std::strtoul(cliScreen, NULL, 10) : 0);
      auto center = wholeScreen.center();
      uint64_t w = max(192, wholeScreen.width()/4), h = w * 108 / 192;
      return intBox3D(center.x - w/2, center.x + w/2, center.y - h/2, center.y + h/2);
    }
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
    ASSERT_TRAP(SDL_Vulkan_CreateSurface(sdlWindow, gpu::getVkInstance(), &vksurface), "Failed to create vk surface");
    surface = vksurface;
    uint32_t presentModeCount = 10;
    vk::PresentModeKHR presentModes[10];//skipping a call to save a malloc, there are only 5 present modes in existance atm
    VK_ASSERT(phys.getSurfacePresentModesKHR(surface, &presentModeCount, presentModes), "failed to enumerate present modes");
    vk::PresentModeKHR mode = vk::PresentModeKHR::eFifo;
    if(contains(presentModes, vk::PresentModeKHR::eMailbox))
      mode = vk::PresentModeKHR::eMailbox;
    else
      WARN("Mailbox present mode not supported, using fifo (vsync always on).");
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
    swapCI.setCompositeAlpha(static_cast<vk::CompositeAlphaFlagBitsKHR>(1 << (ffs(static_cast<vk::CompositeAlphaFlagsKHR::MaskType>(surfCaps.supportedCompositeAlpha)) - 1)));
    auto dev = g.getVkDevice();
    VK_ASSERT(dev.createSwapchainKHR(&swapCI, ALLOCCB, &swap), "could not create swapchain");
    VK_ASSERT(dev.getSwapchainImagesKHR(swap, &swapImageCount, NULL), "Could not count swapchain images.");
    swapImages = std::make_unique<vk::Image[]>(swapImageCount);
    VK_ASSERT(dev.getSwapchainImagesKHR(swap, &swapImageCount, swapImages.get()), "Could not get swapchain images");
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

  void window::addInstanceExtensionsTo(std::vector<const char*>& extensions) {//static
    PROFILEME;
    auto tempWin = SDL_CreateWindow("surface probe", 0, 0, 100, 100, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    unsigned int cnt = 0;
    ASSERT_TRAP(SDL_Vulkan_GetInstanceExtensions(tempWin, &cnt, NULL), "failed to count vk extensions required by sdl");
    const char**names = new const char*[cnt];
    ASSERT_TRAP(SDL_Vulkan_GetInstanceExtensions(tempWin, &cnt, names), "failed to list vk extensions required by sdl");
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

  void window::acquire() {
    PROFILEME;
    VK_ASSERT(gpu::get(gpuIdx).getVkDevice().acquireNextImageKHR(swap, 10000000000 /*10 seconds*/, acquisitionSems[activeSwapSem], VK_NULL_HANDLE, &presentImageIndex), "failed to acquire");
  };

  void window::present(vk::Image src, vk::ImageLayout srcLayout, vk::Offset3D srcSize, vk::SemaphoreSubmitInfo& renderWaitSem) {
    PROFILEME;
    ASSERT_TRAP(srcSize.z == 1, "attempted to present 3d image");
    auto& dev = gpu::get(gpuIdx);
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
      VK_ASSERT(dev.getVkDevice().waitForFences(1, &cmdFences[activeSwapSem], false, 10000000000 /* 10 seconds */),
		"Present fence timeout");
    }
#ifdef WITE_DEBUG_FENCES
    WARN("window wait complete for ", activeSwapSem);
#endif
    VK_ASSERT(dev.getVkDevice().resetFences(1, &cmdFences[activeSwapSem]), "Failed to reset fence (?how?)");
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
      cmd.end();
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
