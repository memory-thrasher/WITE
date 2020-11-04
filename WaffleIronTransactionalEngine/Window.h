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
  Camera* addCamera(WITE::IntBox3D);//TODO full-window camera default
  Camera* getCamera(size_t idx);
  size_t getCameraCount();
  static void pollAllEvents();
  static bool areRendersDone();
  static void renderAll();
  static void presentAll();
  static inline WITE::Window** iterateWindows(size_t &num);
private:
  static std::vector<Window*> windows;
  static std::map<uint32_t, ::Window*> windowsBySdlId;
  void recreateSwapchain();
  inline bool renderEnabled();
  //static std::vector<Window*> windows;//already in super
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
    //VkFramebuffer *framebuffers;
    VkSemaphore semaphore;
  } swapchain;
  std::vector<std::unique_ptr<Camera>> cameras;
  typedef struct {
    VkSwapchainKHR swapchain;
    uint32_t imageIdx;
  } PresentInfoSegment;
  uint32_t render();//returns which swapchain image it uses
};

