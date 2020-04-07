#include "stdafx.h"
#include "Window.h"
#include "Export.h"

Window::Window(size_t display) : displayIdx(display) {
  ScopeLock sl(&vkSingleton.lock, 30000);
  const char* extensions[MAX_EXTENSIONS];
  unsigned extensionCount;
  size_t i, j;
  bool found;
  windows.push_back(this);
  sdlWindow = SDL_CreateWindow(vkSingleton.appInfo.pApplicationName,
			       SDL_WINDOWPOS_CENTERED_DISPLAY((int)display), SDL_WINDOWPOS_CENTERED_DISPLAY((int)display),
			       800, 600, SDL_WINDOW_VULKAN);// | SDL_WINDOW_BORDERLESS
  if (!sdlWindow) CRASH;
  if (!SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensionCount, NULL)) CRASH;
  if (extensionCount > MAX_SDL_EXTENSIONS) CRASH;
  if (!SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensionCount, extensions)) CRASH;
  for (i = 0;i < extensionCount;i++) {
    found = false;
    for (j = 0;!found && j < vkSingleton.extensionCount;j++)
      if (!strcmp(vkSingleton.extensions[j], extensions[i]))
	found = true;
    if (!found) CRASH;
    //this surface requires an instance extension not present in existing instance
  }
  if (!SDL_Vulkan_CreateSurface(sdlWindow, vkSingleton.instance, &surface)) CRASH;
  VkBool32 compatible = false;
  for (i = 0;i < vkSingleton.gpuCount && !compatible;i++) {
    GPU* gpu = vkSingleton.gpus[i];
    Queue *q = gpu->presentQ;
    compatible = q->supports(&surface);
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
  else if(presentQ->gpu->presentQ != presentQ) CRASH;
  uint32_t formatCount;
  VkSurfaceFormatKHR* formats;
  CRASHIFFAIL(vkGetPhysicalDeviceSurfaceFormatsKHR(presentQ->gpu->phys, surface, &formatCount, NULL));
  formats = static_cast<VkSurfaceFormatKHR*>(malloc(sizeof(VkSurfaceFormatKHR) * formatCount));
  if (!formats) CRASH;
  VkResult res = vkGetPhysicalDeviceSurfaceFormatsKHR(presentQ->gpu->phys, surface, &formatCount, formats);
  surfaceFormat = (formats[0].format == VK_FORMAT_UNDEFINED) ?
    VK_FORMAT_B8G8R8A8_UNORM : formats[0].format;//TODO be more picky?
  free(formats);
  CRASHIFFAIL(res);
  CRASHIFFAIL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(presentQ->gpu->phys, surface, &surfaceCaps));
  if(!(surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) CRASH;//if this happens, try VK_SHARING_MODE_CONCURRENT in swapchain.info 
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
    swapchain.info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, NULL, 0, surface,
		       swapchain.imageCount, surfaceFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR, size, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		       VK_SHARING_MODE_EXCLUSIVE, 0, NULL, preTransform, target, VK_PRESENT_MODE_FIFO_KHR,
		       false, NULL };
  } else if(graphicsQ->gpu == presentQ->gpu) {
    uint32_t qfs[2] = { graphicsQ->family, presentQ->family };
    swapchain.info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, NULL, 0, surface,
		       swapchain.imageCount, surfaceFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR, size, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		       VK_SHARING_MODE_CONCURRENT, 2, qfs, preTransform, target, VK_PRESENT_MODE_FIFO_KHR,
		       false, NULL };//fifo = queued vsync
    //swapchain being given q family only establishes allowed access for concurrent share
  } else CRASH;
  CRASHIFFAIL(vkCreateSwapchainKHR(presentQ->gpu->device, &swapchain.info, vkAlloc, &swapchain.chain));
  VkImage* images = new VkImage[swapchain.imageCount];
  CRASHIFFAIL(vkGetSwapchainImagesKHR(presentQ->gpu->device, swapchain.chain, &swapchain.imageCount, images));
  swapchain.images = (BackedImage*)malloc(swapchain.imageCount * sizeof(BackedImage));
  for (i = 0;i < swapchain.imageCount;i++)
    new(&swapchain.images[i])BackedImage(presentQ->gpu, size, surfaceFormat, images[i]);
  delete[] images;
  //framebuffer no longer neccessary
}

Window::~Window() {
  //delete sdlWindow;
}

uint32_t Window::render() {
  const VkClearColorValue SWAP_CLEAR = {{1, 0.27f, 0.7f}, {}};
  VkImageMemoryBarrier discardAndReceive, transformToPresentable;
  size_t i, len;
  auto cam = iterateCameras(&len);
  auto ep = graphicsQ->getComplexPlan();
  uint32_t swapIdx;
  VkCommandBuffer cmd;
  vkAcquireNextImageKHR(graphicsQ->gpu->device, swapchain.chain, 10000000000ul, swapchain.semaphore, NULL, &swapIdx);//TODO determine if this blocks
  ep->queueWaitForSemaphore(swapchain.semaphore);
  for(i = 0;i < len;i++, ++cam)
    (*cam)->render(ep);
  cmd = ep->beginReduce();
  discardAndReceive = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchain.images[swapIdx].image,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE, 0, 0, NULL, 0, NULL, 1, &discardAndReceive);
#ifdef DEBUG
  const VkImageSubresourceLayers subres0 = { VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1 };
  vkCmdClearColorImage(cmd, swapchain.images[swapIdx].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &SWAP_CLEAR, 1, &subres0);
#endif
  for(i = 0;i < len;i++, ++cam)
    (*cam)->blitTo(cmd, swapchain.images[swapIdx].image);
  transformToPresentable = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, graphicsQ->family, presentQ->family, swapchain.images[swapIdx].image,
			     { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE, 0, 0, NULL, 0, NULL, 1, &discardAndReceive);
  return swapIdx;
}

bool Window::isRenderDone() {
  return !graphicsQ->getComplexPlan().isRunning();
}

Camera* Window::iterateCameras(size_t &num) {
  num = cameras.size();
  return cameras.data();
}

std::vector<WITE::Window*>::iterator WITE::Window::iterateWindows(size_t &num) {
  num = windows.size();
  return windows.begin();
}

bool WITE::Window::areRendersDone() {
  auto end = windows.end();
  for(auto w = windows.start();w != end;++w)
    if(!(*w)->isRenderDone()) return false;
  return true;
}

void WITE::Window::renderAll() {
  static std::vector<VkSwapchain> swapchainsPerGpu[MAX_GPUS];
  static std::vector<uint32_t> scIdxPerGpu[MAX_GPUS];
  auto end = windows.end();
  VkSemaphore renderDoneSem;
  size_t doneSems = 1, i, gpu, count;
  GPU* gpu;
  Queue::ExecutionPlan* ep;
  for(i = 0;i < vulkan_singleton.gpuCount;i++) {
    swapchainsPerGpu[i].clear();
    scIdxPerGpu[i].clear();
  }
  for(auto w = windows.start();w != end;++w) {
    i = (*w)->presentQ->gpu->idx;
    swapchainsPerGpu[i].push_back((*w)->swapchain.chain);
    scIdxPerGpu[i].push_back((*w)->render());//reder reduces its execution plan, so only one semaphore need be waited per gpu
  }
  for(i = 0;i < vulkan_singleton.gpuCount;i++) {
    count = scIdxPerGpu.size();
    if(!count) continue;
    gpu = vulkan_singleton.gpus[i];
    ep = gpu->graphicsQ;
    eq->getStateSemaphores(&renderDoneSem, &doneSems);
    if(doneSems != 1) CRASH;
    ep->submit();
    VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL, doneSems, &renderDoneSem,
			    count, swapchainsPerGpu[i].data(), scIdxPerGpu[i].data(), NULL };
    CRASHIFFAIL(vkQueuePresentKHR(gpu->presentQ->queue, &pi));
  }
}

std::unique_ptr<WITE::Window> Window::make(size_t display) {
  return std::make_unique<Window>(display);
}

Camera* addCamera(IntBox3D box) {
  cameras.emplace_back(box, presentQ);
}

Camera* getCamera(size_t idx) {
  return cameras[idx];
}

size_t getCameraCount() {
  return cameras.size();
}
