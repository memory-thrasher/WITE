#include "Window.hpp"
#include "SDL.hpp"
#include "Gpu.hpp"
#include "Queue.hpp"
#include "Math.hpp"
#include "ElasticCommandBuffer.hpp"

namespace WITE::GPU {

  inline Util::IntBox3D intBoxFromSdlRect(SDL_Rect& rect) {
    return Util::IntBox3D(rect.x, rect.x + rect.w, rect.y, rect.y + rect.h);
  };

  inline Util::IntBox3D Window::getScreenBounds(size_t idx) {//static
    SDL_Rect rect;
    SDL_GetDisplayUsableBounds(idx, &rect);
    return intBoxFromSdlRect(rect);
  };

  Util::IntBox3D getSplashBox() {
    auto wholeScreen = Window::getScreenBounds(0);
    auto center = wholeScreen.center();
    uint64_t w = Util::max(192, wholeScreen.width()/4), h = w * 192 / 108;
    return Util::IntBox3D(center.x - w/2, center.x + w/2, center.y - h/2, center.y + h/2);
  };

  Window::Window() : Window(getSplashBox()) {};

  Window::Window(Util::IntBox3D box) : Window(box.minx, box.miny, box.width(), box.height()) {};

  //a rendered image is always blitted to the window's swapchain image (for now. HUD overlay might make sense to take the rendered image as an input attachment and color attach the swapchain image. Or maybe the hud elements will be rendered to a mostly transparent image and the two or more are composited here by alpha)
  Window::Window(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {
    swapCI.setMinImageCount(3).setImageColorSpace(vk::ColorSpaceKHR::ePassThroughExt).setImageArrayLayers(1)
      .setImageUsage(vk::ImageUsageFlagBits::eTransferDst).setImageSharingMode(vk::SharingMode::eExclusive)
      .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity).setPresentMode(vk::PresentModeKHR::eMailbox)
      .setClipped(true);
    window = SDL_CreateWindow("WITE", x, y, w, h, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
    VkSurfaceKHR ts;
    ASSERT_TRAP(SDL_Vulkan_CreateSurface(window, Gpu::getVkInstance(), &ts), "Failed to create vk surface");
    swapCI.setSurface(ts);
    //now that the window exists, we can ask each device if it can render to it
    vk::Bool32 supported = false;
    for(size_t i = 0;i < Gpu::getGpuCount() && !presentQueue;i++) {
      Gpu& g = Gpu::get(i);
      Queue* fams[2] = { g.getGraphics(), g.getTransfer() };
      for(size_t j = 0;j < 2 && !presentQueue;j++) {
	g->getPhysical().getSurfaceSupportKHR(fams[j]->family, surface, &supported);
	if(supported) {
	  presentQueue = fams[j];
	}
      }
    }
    ASSERT_TRAP(presentQueue != NULL, "No device can render to this window!");
    Gpu* gpu = presentQueue->getGpu();
    uint32_t formatCount;
    auto phys = gpu->getPhysical();
    {
      VK_ASSERT(phys.getSurfaceFormatsKHR(surface, &formatCount, NULL), "Could not count surface formats");
      auto formats = std::make_unique<vk::SurfaceFormat[]>(formatCount);
      VK_ASSERT(phys.getSurfaceFormatsKHR(surface, &formatCount, formats.get()), "Could not enumerate surface formats");
      swapCI.imageFormat = formats[0].format;
      if(swapCI.imageFormat == vk::Format::eUndefined)
	swapCI.imageFormat = vk::Format::eB8G8R8_UNORM;
    }
    VK_ASSERT(phs.getSurfaceCapabilitiesKHR(surface, &surfCaps), "Could not fetch surface capabilities.");
    swapCI.setImageExtent({
	Util::clamp(w, surfCaps.minImageExtent.width,  surfCaps.maxImageExtent.width),
	Util::clamp(h, surfCaps.minImageExtent.height, surfCaps.maxImageExtent.height)
      });
    swapCI.setCompositeAlpha(static_cast<vk::CompositeAlphaFlagBitsKHR>(1 << (ffs(surfCaps.supportedCompositeAlpha) - 1)));
    auto dev = gpu->getVkDevice();
    VK_ASSERT(dev.createSwapchainKHR(&swapCI, ALLOCCB, &swap), "could not create swapchain");
    VK_ASSERT(dev.getSwapchainImagesKHR(swapchain, &swapImageCount, NULL), "Could not count swapchain images.");
    swapImages = std::make_unique<vk::Image>(swapImageCount);
    VK_ASSERT(dev.getSwapchainImagesKHR(swapchain, &swapImageCount, swapImages.get()), "Could not get swapchain images");
    if(!presentQueue->supportsTransfer())
      transferQueue = presentQueue->getGpu()->getTransfer();
  };

  void Window::addInstanceExtensionsTo(std::vector<const char*>& extensions) {//static
    auto tempWin = SDL_CreateWindow(0, 0, 100, 100, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    unsigned int cnt = 0;
    ASSERT_TRAP(SDL_Vulkan_GetInstanceExtensions(tempWin, &cnt, NULL), "failed to count vk extensions required by sdl");
    const char* names = new char[cnt];
    ASSERT_TRAP(SDL_Vulkan_GetInstanceExtensions(tempWin, &cnt, names), "failed to list vk extensions required by sdl");
    SDL_DestroyWindow(tempWin);
    extensions.reserve(extensions.size() + cnt);
    for(size_t i = 0;i < cnt;i++)
      extensions.push_back(names[i]);
  };

  void Window::drawImpl(ImageBase* img, Semaphore* sem) {
    ElasticCommandBuffer cmd = (transferQueue ? transferQueue : presentQueue)->createBatch();
    //
    if(swapCI.extent == img->getVkSize()) {
      //same size, use the easy copy
      cmd->copyImage();
    } else {
      //blit to scale but potentially crop to maintain aspect ratio
      //TODO
    }
  };

}
