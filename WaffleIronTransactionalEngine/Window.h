#pragma once

#include "stdafx.h"
#include "BackedImage.h"
#include "Camera.h"

class Window : public WITE::Window {
public:
  Window(size_t display = 0);
  ~Window();
  bool isRenderDone();
  std::vector<Camera*>::iterator iterateCameras(size_t &num);
  void setSize(uint32_t width, uint32_t height);
  void setBounds(IntBox3D bounds);
  void setLocation(int32_t x, int32_t y, uint32_t w, uint32_t h);
  Camera* addCamera(IntBox3D);
  Camera* getCamera(size_t idx);
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
  std::vector<Camera> cameras;
  typedef struct {
    VkSwapchainKHR swapchain;
    uint32_t imageIdx;
  } PresentInfoSegment;
  uint32_t render();//returns which gpu it uses
};

