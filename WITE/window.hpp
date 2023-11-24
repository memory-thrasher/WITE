#pragma once

#include <vector>

#include "wite_vulkan.hpp"
#include "templateStructs.hpp"
#include "onionUtils.hpp"
#include "math.hpp"
#include "image.hpp"

class SDL_Window;//prototype for handle

namespace WITE {

  class window {
  private:
    //const uint32_t x, y, w, h;//resize = recreate the window.
    vk::SwapchainCreateInfoKHR swapCI;
    SDL_Window* sdlWindow;
    vk::SurfaceCapabilitiesKHR surfCaps;
    gpu* presentDevice;
    vk::SwapchainKHR swap;
    uint32_t swapImageCount;
    std::unique_ptr<vk::Image[]> swapImages;

  public:
    static void addInstanceExtensionsTo(std::vector<const char*>& extensions);
    static intBox3D getScreenBounds(size_t idx = 0);

    static constexpr imageRequirements::flowStep presentationFlow = {
      .onionId = NONE,
      .shaderId = NONE,
      .layout = vk::ImageLayout::eTransferSrcOptimal,
      .stages = vk::PipelineStageFlagBits2::eBlit,
      .access = vk::AccessFlagBits2::eTransferRead
    };
    static constexpr imageRequirements presentationRequirements {
      .usage = vk::ImageUsageFlagBits::eTransferSrc,
      .flow = { presentationFlow },
      .flowCount = 1
    };
    static constexpr imageRequirements presentationSuggestions {
      .usage = vk::ImageUsageFlagBits::eTransferSrc,
      .flow = { presentationFlow },
      .flowCount = 1,
      .dimensions = 2,
      .arrayLayers = 1,
      .mipLevels = 1
    };

    window();//default is spalsh-screen-like automatically determined
    window(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    window(intBox3D box);//z axis ignored

    vk::Extent2D getSize();//fetch from the os. Note that this may be different from the requested size in various legit cases.
    inline glm::uvec2 getGlSize() {
      auto vke = getSize();
      return { vke.width, vke.height };
    };

    template<imageRequirements R> void blit(image<R>&) {
      static_assert(R.dimensions == 2);//TODO option to split a 3d image?
      static_assert(R.usage & vk::ImageUsageFlagBits::eTransferSrc);
      static_assert((R.imageFlags & imageFlags_e::eAlwaysGeneralLayout) ||
		    R.flow[R.flowCount-1].layout == vk::ImageLayout::eGeneral ||
		    R.flow[R.flowCount-1].layout == vk::ImageLayout::eTransferSrcOptimal ||
		    R.flow[R.flowCount-1].layout == vk::ImageLayout::eSharedPresentKHR);
      #error TODO
    };

  };

}

