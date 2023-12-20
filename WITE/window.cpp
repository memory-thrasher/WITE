#include "window.hpp"
#include "SDL.hpp"
#include "gpu.hpp"

namespace WITE {

  inline intBox3D intBoxFromSdlRect(SDL_Rect& rect) {
    return intBox3D(rect.x, rect.x + rect.w, rect.y, rect.y + rect.h);
  };

  inline intBox3D window::getScreenBounds(size_t idx) {//static
    SDL_Rect rect;
    SDL_GetDisplayUsableBounds(idx, &rect);
    return intBoxFromSdlRect(rect);
  };

  intBox3D getSplashBox() {
    auto wholeScreen = window::getScreenBounds(0);
    auto center = wholeScreen.center();
    uint64_t w = max(192, wholeScreen.width()/4), h = w * 108 / 192;
    return intBox3D(center.x - w/2, center.x + w/2, center.y - h/2, center.y + h/2);
  };

  window::window(size_t gpuIdx) : window(gpuIdx, getSplashBox()) {};

  window::window(size_t gpuIdx, intBox3D box) : gpuIdx(gpuIdx) {
    //a rendered image is always blitted to the window's swapchain image
    x = (uint32_t)box.minx;
    y = (uint32_t)box.miny;
    w = (uint32_t)box.width();
    h = (uint32_t)box.height();
    swapCI.setMinImageCount(3).setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear).setImageArrayLayers(1)
      .setImageUsage(vk::ImageUsageFlagBits::eTransferDst).setImageSharingMode(vk::SharingMode::eExclusive)
      .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity).setPresentMode(vk::PresentModeKHR::eMailbox)
      .setClipped(true);
    sdlWindow = SDL_CreateWindow("WITE", x, y, w, h, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
    VkSurfaceKHR vksurface;
    ASSERT_TRAP(SDL_Vulkan_CreateSurface(sdlWindow, gpu::getVkInstance(), &vksurface), "Failed to create vk surface");
    surface = vksurface;
    swapCI.setSurface(surface);
    //now that the window exists, we can ask each device if it can render to it
    vk::Bool32 supported = false;
    gpu& g = gpu::get(gpuIdx);
    auto phys = g.getPhysical();
    VK_ASSERT(phys.getSurfaceSupportKHR(g.getQueueFam(), surface, &supported), "failed to query surface support");
    ASSERT_TRAP(supported, "Device cannot render to this window!");
    uint32_t formatCount;
    {
      VK_ASSERT(phys.getSurfaceFormatsKHR(surface, &formatCount, NULL), "Could not count surface formats");
      auto formats = std::make_unique<vk::SurfaceFormatKHR[]>(formatCount);
      VK_ASSERT(phys.getSurfaceFormatsKHR(surface, &formatCount, formats.get()), "Could not enumerate surface formats");
      swapCI.imageFormat = formats[0].format;
      if(swapCI.imageFormat == vk::Format::eUndefined)//vk standard meaning "any is fine"
	swapCI.imageFormat = vk::Format::eB8G8R8Unorm;
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
    for(size_t i = 0;i < swapSemCount;i++) {
      VK_ASSERT(dev->getVkDevice().createSemaphore(&semCI, ALLOCCB, &acquisitionSems[i]), "failed to create semaphore");
      VK_ASSERT(dev->getVkDevice().createSemaphore(&semCI, ALLOCCB, &blitSems[i]), "failed to create semaphore");
    }
    const vk::CommandPoolCreateInfo poolCI(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, g.getQueueFam());
    VK_ASSERT(dev.createCommandPool(&poolCI, ALLOCCB, &cmdPool), "failed to allocate command pool");
    vk::CommandBufferAllocateInfo allocInfo(cmdPool, vk::CommandBufferLevel::ePrimary, swapSemCount);
    cmds = std::make_unique<vk::CommandBuffer[]>(swapSemCount);
    VK_ASSERT(dev.allocateCommandBuffers(&allocInfo, cmds.get()), "failed to allocate command buffer");
  };

  void window::addInstanceExtensionsTo(std::vector<const char*>& extensions) {//static
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

  vk::Extent2D window::getSize() {
    int w, h;
    SDL_Vulkan_GetDrawableSize(sdlWindow, &w, &h);
    return { (uint32_t)w, (uint32_t)h };
  };

  void acquire() {
    vk::AcquireNextImageInfoKHR acquireInfo { swap, 10000000000 /*10 seconds*/, acquisitionSems[activeSwapSem], NULL };
    gpt::get(gpuIdx).getVkDevice().acquireNextImage2KHR(&acquireInfo, &presentImageIndex);
  };

  void present(vk::Image src, vk::ImageLayout srcLayout, vk::Offset3D size, vk::SemaphoreSubmitInfo& renderWaitSem) {
    ASSERT_TRAP(size.z == 1);
    auto dst = swapImages[presentImageIndex];
    vk::ImageMemoryBarrier2 barrier1 {
      vk::PipelineStageFlags2::eNone, vk::AccessFlags2::eNone,
      vk::PipelineStageFlags2::eTransfer, vk::AccessFlags2::eTransferWrite,
      vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
      {}, {}, dst, { vk::ImageAspectFlags::eColor, 0, 1, 0, 1 }
    }, barrier2 {
      vk::PipelineStageFlags2::eTransfer, vk::AccessFlags2::eTransferWrite,
      vk::PipelineStageFlags2::eNone, vk::AccessFlags2::eNone,
      vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR,
      {}, {}, dst, { vk::ImageAspectFlags::eColor, 0, 1, 0, 1 }
    };
    vk::DependencyInfo di { {}, 0, NULL, 0, NULL, 1, &barrier1 };
    vk::CommandBuffer cmd = cmds[activeSwapSem];
    vk::CommandBufferBeginInfo begin { vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    cmd.begin(&begin);
    cmd.pipelineBarrier2(&di);
    vk::ImageBlit2 region { { vk::ImageAspectFlags::eColor, 0, 0, 1 }, {{0, 0, 0}, srcSize},
			    { vk::ImageAspectFlags::eColor, 0, 0, 1 }, {{0, 0, 0}, { x, y, 1 }} };
    vk::BlitImageInfo2 blit { src, srcLayout, dst, vk::ImageLayout::eTransferDstOptimal, 1, &region, vk::Filter::Nearest };
    cmd.blitImage2(&blit);
    di.pImageMemoryBarriers = &barrier2;
    cmd.pipelineBarrier2(&di);
    vk::SemaphoreSubmitInfo waits[2] { renderWaitSem
	{ acquisitionSems[activeSwapSem], 0, vk::PipelineStageFlagBits::eNone }
    }, signal { blitSems[activeSwapSem], 0, vk::PipelineStageFlagBits::eNone };
    vk::CommandBufferSubmitInfo cmdSubmitInfo(cmd);
    vk::SubmitInfo2 submit({}, 2, waits, 1, &cmdSubmitInfo, 1, &signal);
    VK_ASSERT(gpt::get(gpuIdx).getQueue().submit2(1, &submit, NULL), "failed to submit command buffer");
    vk::PresentInfoKHR presentInfo { 1, &blitSems[activeSwapSem], 1, &swap, &presentImageIndex, NULL };
    VK_ASSERT(gpt::get(gpuIdx).getVkDevice().presentKHR(&presentInfo), "Present failed");
    //TODO handle suboptimal return. Send resize request to target_t?
  };

}
