#include "stdafx.h"
#include "Window.h"
#include "Export.h"
#include "GPU.h"
#include "WMath.h"

std::vector<WITE::Window*> WITE::Window::windows;

Window::Window(size_t display) : WITE::Window() {
  displayIdx = display;
  WITE::ScopeLock sl(&vkSingleton.lock);
  const char* extensions[MAX_EXTENSIONS];
  unsigned extensionCount;
  size_t i, j;
  bool found;
  windows.push_back(this);
  sdlWindow = SDL_CreateWindow(vkSingleton.appInfo.pApplicationName,
			       SDL_WINDOWPOS_CENTERED_DISPLAY((int)display), SDL_WINDOWPOS_CENTERED_DISPLAY((int)display),
			       800, 600, SDL_WINDOW_VULKAN);// | SDL_WINDOW_BORDERLESS
  if (!sdlWindow) CRASH("Filed to create window\n");
  if (!SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensionCount, NULL)) CRASH("Failed to count extensions required by sdl window\n");
  if (extensionCount > MAX_SDL_EXTENSIONS)
    CRASH("Fail: sdl requires more extensions than we have room for. MAX_SDL_EXTENSIONS %d > %d\n", MAX_SDL_EXTENSIONS, extensionCount);
  if (!SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensionCount, extensions)) CRASH("Failed to enumerate extensions required by sdl window\n");
  for (i = 0;i < extensionCount;i++) {
    found = false;
    for (j = 0;!found && j < vkSingleton.extensionCount;j++)
      if (!strcmp(vkSingleton.extensions[j], extensions[i]))
	found = true;
    if (!found) CRASH("Extension required by sdl was not found: %s\n", extensions[i]);
    //this surface requires an instance extension not present in existing instance
  }
  if (!SDL_Vulkan_CreateSurface(sdlWindow, vkSingleton.instance, &surface)) CRASH("Failed to create surface for window\n");
  VkBool32 compatible = false;
  for (i = 0;i < vkSingleton.gpuCount && !compatible;i++) {
    GPU* gpu = vkSingleton.gpus[i];
    Queue *q = gpu->presentQ;
    compatible = q && q->supports(&surface);
    if (compatible) presentQ = q;
  }
  for (i = 0;i < vkSingleton.gpuCount && !compatible;i++) {
    GPU* gpu = vkSingleton.gpus[i];
    for (j = 0;j < gpu->queueCount && !compatible;j++) {
      Queue *q = gpu->queues[j];
      compatible = q->supports(&surface);
      if (compatible) presentQ = q;
    }
  }
  graphicsQ = presentQ->gpu->graphicsQ;
  if(!presentQ->gpu->presentQ) presentQ->gpu->presentQ = presentQ;
  //docs unclear, but hopefully a gpu queue that can present any surface can also support all surfaces on it.
  //This is assumed later in render code, so assert it here:
  else if(presentQ->gpu->presentQ != presentQ) CRASH("Not Yet Implemented: different windows on the same gpu require different present queues.\n");
  uint32_t formatCount;
  VkSurfaceFormatKHR* formats;
  CRASHIFFAIL(vkGetPhysicalDeviceSurfaceFormatsKHR(presentQ->gpu->phys, surface, &formatCount, NULL));
  formats = static_cast<VkSurfaceFormatKHR*>(malloc(sizeof(VkSurfaceFormatKHR) * formatCount));
  if (!formats) CRASH("Out of ram when making window. This really shouldn't happen.\n");
  VkResult res = vkGetPhysicalDeviceSurfaceFormatsKHR(presentQ->gpu->phys, surface, &formatCount, formats);
  surfaceFormat = (formats[0].format == VK_FORMAT_UNDEFINED) ?
    VK_FORMAT_B8G8R8A8_UNORM : formats[0].format;//TODO be more picky?
  free(formats);
  CRASHIFFAIL(res);
  CRASHIFFAIL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(presentQ->gpu->phys, surface, &surfaceCaps));
  if(!(surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    CRASH("Surfface swapchain image does not support transfer dst\n");//if this happens, try VK_SHARING_MODE_CONCURRENT in swapchain.info 
  size.width = glm::clamp<uint32_t>(surfaceCaps.currentExtent.width, surfaceCaps.minImageExtent.width,
				    surfaceCaps.maxImageExtent.width);
  size.height = glm::clamp<uint32_t>(surfaceCaps.currentExtent.height, surfaceCaps.minImageExtent.height,
				     surfaceCaps.maxImageExtent.height);
  swapchain.imageCount = glm::clamp<uint32_t>(2, surfaceCaps.minImageCount, surfaceCaps.maxImageCount);
  VkSurfaceTransformFlagBitsKHR preTransform =
    (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ?
    VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfaceCaps.currentTransform;
  static const VkCompositeAlphaFlagBitsKHR preferred[] =
    { VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR };
  VkCompositeAlphaFlagBitsKHR target = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
  for (i = 0;i < sizeof(VkCompositeAlphaFlagsKHR);i++)
    if (surfaceCaps.supportedCompositeAlpha & preferred[i]) {
      target = preferred[i];
      break;
    }
  if (graphicsQ == presentQ) {//TODO use imageusageflags if sampled or transfered
    swapchain.info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, VK_NULL_HANDLE, 0, surface,
		       swapchain.imageCount, surfaceFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR, size, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, VK_NULL_HANDLE, preTransform, target, VK_PRESENT_MODE_FIFO_KHR, false, VK_NULL_HANDLE };
  } else if(graphicsQ->gpu == presentQ->gpu) {
    uint32_t qfs[2] = { graphicsQ->family, presentQ->family };
    swapchain.info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, VK_NULL_HANDLE, 0, surface,
		       swapchain.imageCount, surfaceFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR, size, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_CONCURRENT, 2, qfs, preTransform, target, VK_PRESENT_MODE_FIFO_KHR, false, VK_NULL_HANDLE };//fifo = queued vsync
    //swapchain being given q family only establishes allowed access for concurrent share
  } else CRASH("Not Yet Implemented: gpu that does render cannot present\n");
  CRASHIFFAIL(vkCreateSwapchainKHR(presentQ->gpu->device, &swapchain.info, vkAlloc, &swapchain.chain));
  VkImage* images = new VkImage[swapchain.imageCount];
  CRASHIFFAIL(vkGetSwapchainImagesKHR(presentQ->gpu->device, swapchain.chain, &swapchain.imageCount, images));
  swapchain.images = (BackedImage*)malloc(swapchain.imageCount * sizeof(BackedImage));
  for (i = 0;i < swapchain.imageCount;i++)
    new(&swapchain.images[i])BackedImage(presentQ->gpu, size, surfaceFormat, images[i]);//TODO make semaphore?
  delete[] images;
  VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0 };
  vkCreateSemaphore(presentQ->gpu->device, &semInfo, vkAlloc, &swapchain.semaphore);
}

Window::~Window() {
  //delete sdlWindow;
  //TODO
}

void Window::recreateSwapchain() {
  size_t i;
  delete[] swapchain.images;
  //copied from constructor:
  CRASHIFFAIL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(presentQ->gpu->phys, surface, &surfaceCaps));//read violation here; vert low address
  if(!(surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    CRASH("Surfface swapchain image does not support transfer dst\n");
  size.width = glm::clamp<uint32_t>(surfaceCaps.currentExtent.width, surfaceCaps.minImageExtent.width,
    surfaceCaps.maxImageExtent.width);
  size.height = glm::clamp<uint32_t>(surfaceCaps.currentExtent.height, surfaceCaps.minImageExtent.height,
    surfaceCaps.maxImageExtent.height);
  swapchain.info.imageExtent = size;
  swapchain.info.oldSwapchain = swapchain.chain;
  CRASHIFFAIL(vkCreateSwapchainKHR(presentQ->gpu->device, &swapchain.info, vkAlloc, &swapchain.chain));
  VkImage* images = new VkImage[swapchain.imageCount];
  CRASHIFFAIL(vkGetSwapchainImagesKHR(presentQ->gpu->device, swapchain.chain, &swapchain.imageCount, images));
  swapchain.images = (BackedImage*)malloc(swapchain.imageCount * sizeof(BackedImage));
  for(i = 0;i < swapchain.imageCount;i++)
    new(&swapchain.images[i])BackedImage(presentQ->gpu, size, surfaceFormat, images[i]);//TODO make semaphore?
  delete[] images;
  //vkDestroySwapchainKHR(presentQ->gpu->device, swapchain.chain, NULL);//TODO
}

uint32_t Window::render() {
  const VkClearColorValue SWAP_CLEAR = {{1, 0.27f, 0.7f}};
  VkImageMemoryBarrier discardAndReceive, transformToPresentable;
  size_t i, len = cameras.size();
  auto ep = graphicsQ->getComplexPlan();
  uint32_t swapIdx;
  VkCommandBuffer cmd;
  CRASHIFFAIL(vkAcquireNextImageKHR(graphicsQ->gpu->device, swapchain.chain, 10000000000ul, swapchain.semaphore, VK_NULL_HANDLE, &swapIdx), 0);//TODO determine if this blocks
  ep->queueWaitForSemaphore(swapchain.semaphore);
  for(i = 0;i < len;i++)
    cameras[i]->render(ep);
  cmd = ep->beginReduce();
  discardAndReceive = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchain.images[swapIdx].getImage(), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &discardAndReceive);
#ifdef _DEBUG
  const VkImageSubresourceRange subres0 = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
  vkCmdClearColorImage(cmd, swapchain.images[swapIdx].getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &SWAP_CLEAR, 1, &subres0);
#endif
  for(i = 0;i < len;i++)
    cameras[i]->blitTo(cmd, swapchain.images[swapIdx].getImage());
  transformToPresentable = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, graphicsQ->family, presentQ->family, swapchain.images[swapIdx].getImage(), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &transformToPresentable);//Also works top of pipe
  return swapIdx;
}

bool Window::isRenderDone() {
  return !graphicsQ->getComplexPlan()->isRunning();
}

std::vector<WITE::Window*>::iterator WITE::Window::iterateWindows(size_t &num) {
  num = windows.size();
  return windows.begin();
}

static std::vector<uint32_t> scIdxPerGpu[MAX_GPUS];//ONLY use from master thread
static std::vector<VkSwapchainKHR> swapchainsPerGpu[MAX_GPUS];

bool Window::areRendersDone() {
  for(size_t i = 0;i < vkSingleton.gpuCount;i++)
    if(scIdxPerGpu[i].size() && vkSingleton.gpus[i]->graphicsQ->getComplexPlan()->isRunning()) return false;
  return true;
}

void Window::renderAll() {
  auto end = windows.end();
  size_t i, count;
  Window* w;
  GPU* gpu;
  std::shared_ptr<Queue::ExecutionPlan> ep;
  for(i = 0;i < vkSingleton.gpuCount;i++) {
    swapchainsPerGpu[i].clear();
    scIdxPerGpu[i].clear();
  }
  for(auto ww = windows.begin();ww != end;++ww) {
    w = (Window*)(*ww);
    i = w->presentQ->gpu->idx;
    swapchainsPerGpu[i].push_back(w->swapchain.chain);
    TIME(scIdxPerGpu[i].push_back(w->render()), 2, "\tRender window: %llu\n");
    //reder reduces its execution plan, so only one semaphore need be waited per gpu
  }
  for(i = 0;i < vkSingleton.gpuCount;i++) {
    count = scIdxPerGpu[i].size();
    if(!count) continue;
    gpu = vkSingleton.gpus[i];
    ep = gpu->graphicsQ->getComplexPlan();
    TIME(ep->submit(), 2, "\tSubmit for gpu %d: %llu\n", i);
  }
}

void Window::presentAll() {
  size_t i, count;
  GPU* gpu;
  decltype(windows)::iterator end;
  for(i = 0;i < vkSingleton.gpuCount;i++) {
    count = scIdxPerGpu[i].size();
    if(!count) continue;
    gpu = vkSingleton.gpus[i];
    VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL, 0, NULL, (uint32_t)count, swapchainsPerGpu[i].data(), scIdxPerGpu[i].data(), NULL };
    VkResult res;
    TIME(res = vkQueuePresentKHR(gpu->presentQ->queue, &pi), 2, "\tPresent: %llu\n");
    switch(res) {
    case VK_SUCCESS: break;
    case VK_SUBOPTIMAL_KHR://docs say recreating the swapchain is the right answer
    case VK_ERROR_OUT_OF_DATE_KHR:
      LOG("Recreating swapchains becuase present returned: %I64d\n", res);
      end = windows.end();
      for(auto ww = windows.begin();ww != end;++ww) {
        ((::Window*)*ww)->recreateSwapchain();
      }
      break;
    default:
      CRASH("Present failed with return code: %I64d\n", res);
    }
  }
}

std::unique_ptr<WITE::Window> WITE::Window::make(size_t display) {
  return std::make_unique<::Window>(display);
}

Camera* Window::addCamera(WITE::IntBox3D box) {
  Camera* ret = new Camera(box, graphicsQ, presentQ);
  cameras.emplace_back(ret);
  return ret;
}

Camera* Window::getCamera(size_t idx) {
  return cameras[idx].get();
}

size_t Window::getCameraCount() {
  return cameras.size();
}

WITE::IntBox3D Window::getBounds() {
  VkExtent2D* e = &swapchain.info.imageExtent;//TODO get from image
  return WITE::IntBox3D(0, e->width, 0, e->height);//TODO get offset
}

void Window::setBounds(WITE::IntBox3D bounds) {
  setLocation(bounds.minx, bounds.miny, bounds.width(), bounds.height());
}

void Window::setLocation(int32_t x, int32_t y, uint32_t w, uint32_t h) {
  setLocation(x, y);
  setSize(w, h);
}

void Window::setLocation(int32_t x, int32_t y) {
  SDL_SetWindowPosition(sdlWindow, x, y);
}

void Window::setSize(uint32_t width, uint32_t height) {
  SDL_SetWindowSize(sdlWindow, width, height);
}
