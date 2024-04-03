#pragma once

#include <vector>
#include <memory>

#include "wite_vulkan.hpp"
// #include "onionTemplateStructs.hpp"
// #include "onionUtils.hpp"
#include "math.hpp"
// #include "image.hpp"

class SDL_Window;//prototype for handle

namespace WITE {

  class window {
  private:
    uint32_t x, y, w, h;
    vk::SwapchainCreateInfoKHR swapCI;
    SDL_Window* sdlWindow;
    vk::SurfaceCapabilitiesKHR surfCaps;
    vk::SurfaceKHR surface;
    vk::SwapchainKHR swap;
    vk::CommandPool cmdPool;
    uint32_t swapImageCount;
    size_t gpuIdx;
    std::unique_ptr<vk::Image[]> swapImages;
    std::unique_ptr<vk::Semaphore[]> acquisitionSems, blitSems;//NOT timeline (acquire and present don't support timeline sems)
    std::unique_ptr<vk::CommandBuffer[]> cmds;
    std::unique_ptr<vk::Fence[]> cmdFences;
    size_t activeSwapSem = 0, swapSemCount;

  public:
    static void addInstanceExtensionsTo(std::vector<const char*>& extensions);
    static intBox3D getScreenBounds(size_t idx = 0);
    static vk::PresentModeKHR getPreferredPresentMode();

    std::atomic_uint64_t framesSkipped;

    window(size_t gpuIdx);//default is spalsh-screen-like automatically determined
    window(size_t gpuIdx, intBox3D box);//z axis ignored
    ~window();

    vk::Extent2D getSize();//fetch from the os. Note that this may be different from the requested size in various legit cases.
    vk::Extent3D getSize3D();
    glm::uvec2 getUvecSize();
    glm::vec2 getVecSize();
    void present(vk::Image src, vk::ImageLayout srcLayout, vk::Offset3D size, vk::SemaphoreSubmitInfo& renderWaitSem);
    void resize();//TODO
    void hide();
    void show();

  };

}

