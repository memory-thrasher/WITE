#pragma once

#include <vector>

#include "Vulkan.hpp"
//#include "PlatformSpecific.hpp"
#include "Math.hpp"
#include "Image.hpp"

class SDL_Window;//prototype for handle

namespace WITE::GPU {

  class Queue;
  class Semaphore;

  class Window {
  private:
    //const uint32_t x, y, w, h;//resize = recreate the window.
    vk::SwapchainCreateInfoKHR swapCI;
    SDL_Window* window;
    vk::SurfaceCapabilitiesKHR surfCaps;
    Queue* presentQueue;
    vk::SwapchainKHR swap;
    std::unique_ptr<vk::Image[]> swapImages;
    uint32_t swapImageCount;
    void drawImpl(ImageBase* img);
    std::atomic_uint64_t queuedRenders;
  public:
    static void addInstanceExtensionsTo(std::vector<const char*>& extensions);
    static Util::IntBox3D getScreenBounds(size_t idx = 0);
    Window();//default is spalsh-screen-like automatically determined
    Window(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    Window(Util::IntBox3D box);//z axis ignored

    vk::Extent2D getSize();//fetch from the os. Note that this may be different from the requested size in various legit cases.

    static constexpr bool imageIsPresentable(ImageSlotData ISD) {
      //TODO overloads that allow: multi-sample resolution, cubemap cropping, 3D image intersection etc as needed
      return ISD.dimensions == 2 && ISD.samples == 1 && (ISD.usage | ISD.externalUsage) & GpuResource::USAGE_TRANSFER &&
	ISD.components >= 3;
    };

    uint64_t queueDepth() {
      return queuedRenders;
    };

    //NOTE: Image's instance on this gpu MUST be in transfer_src layout. Generally called by RenderTarget.
    template<ImageSlotData ISD> inline void draw(Image<ISD>* img)
      requires (imageIsPresentable(ISD))
    {
      drawImpl(img);
    };
  };

}

