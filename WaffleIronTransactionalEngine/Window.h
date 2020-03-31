#pragma once

#include "stdafx.h"
#include "VBackedImage.h"
#include "VCamera.h"

class Window : public WITE::Window {
public:
  Window(size_t display = 0);
  ~Window();
  bool isRenderDone();
  std::vector<Camera*>::iterator iterateCameras(size_t &num);
private:
  //static std::vector<Window*> windows;//already in super
  SDL_Window * sdlWindow;
  size_t displayIdx;
  VkSurfaceKHR surface;
  VkFormat surfaceFormat;
  Queue *graphicsQ, *presentQ;
  VkSurfaceCapabilitiesKHR surfaceCaps;
  VkExtent2D size;
  struct swapchain_t {
    uint32_t imageCount;
    VkSwapchainCreateInfoKHR info;
    VkSwapchainKHR chain;
    BackedImage* images;
    VkFramebuffer *framebuffers;
    VkSemaphore semaphore;
  } swapchain;
  BackedImage* depthBuffer;//TODO move to cam (and also store formate)
  VkRenderPass renderPass;//TODO move to shader
  //VBackedBuffer* uniformBuffer;
  //VkDescriptorSetLayout descLayout;
  //VkPipelineLayout pipelineLayout;
  std::vector<Camera*> cameras;
  VkFense fense = NULL;//null if and only if there is no active render
  typedef struct {
    VkSwapchainKHR swapchain;
    uint32_t imageIdx;
  } PresentInfoSegment;
  uint32_t render();//returns which gpu it uses
};

