#pragma once

class Window : public WITE::Window {
public:
  Window(size_t display = 0);
  ~Window();
  bool isRenderDone();
  void setSize(uint32_t width, uint32_t height);
  void setBounds(WITE::IntBox3D bounds);
  void setLocation(int32_t x, int32_t y, uint32_t w, uint32_t h);
  void setLocation(int32_t x, int32_t y);
  WITE::IntBox3D getBounds();
  Camera* addCamera(WITE::IntBox3D, std::shared_ptr<WITE::ImageSource>);
  Camera* getCamera(size_t idx);
  size_t getCameraCount();
  WITE::Queue* getGraphicsQueue();
  static void pollAllEvents();
  static bool areRendersDone();
  void present();
  static void presentAll();
  static inline WITE::Window** iterateWindows(size_t &num);
private:
  static std::vector<Window*> windows;
  static std::map<uint32_t, ::Window*> windowsBySdlId;
  void recreateSwapchain();
  inline bool renderEnabled();
  SDL_Window * sdlWindow;
  uint32_t sdlWindowId;
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
    VkSemaphore semaphore;
  } swapchain;
  std::vector<std::unique_ptr<Camera>> cameras;
};

