/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream> //for debug only
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <memory>
#include <unistd.h>

#include "../build/Tests/basicShader.frag.spv.h"
#include "../build/Tests/basicShader.vert.spv.h"

struct Vertex {
    float posX, posY, posZ;  // Position data
};

#define XYZ(_x_, _y_, _z_) (_x_), (_y_), (_z_)

static const Vertex g_vb_texture_Data[] = {
    // left face
    {XYZ(-1, -1, -1)},
    {XYZ(-1, 1, 1)},
    {XYZ(-1, -1, 1)},
    {XYZ(-1, 1, 1)},
    {XYZ(-1, -1, -1)},
    {XYZ(-1, 1, -1)},
    // front face
    {XYZ(-1, -1, -1)},
    {XYZ(1, -1, -1)},
    {XYZ(1, 1, -1)},
    {XYZ(-1, -1, -1)},
    {XYZ(1, 1, -1)},
    {XYZ(-1, 1, -1)},
    // top face
    {XYZ(-1, -1, -1)},
    {XYZ(1, -1, 1)},
    {XYZ(1, -1, -1)},
    {XYZ(-1, -1, -1)},
    {XYZ(-1, -1, 1)},
    {XYZ(1, -1, 1)},
    // bottom face
    {XYZ(-1, 1, -1)},
    {XYZ(1, 1, 1)},
    {XYZ(-1, 1, 1)},
    {XYZ(-1, 1, -1)},
    {XYZ(1, 1, -1)},
    {XYZ(1, 1, 1)},
    // right face
    {XYZ(1, 1, -1)},
    {XYZ(1, -1, 1)},
    {XYZ(1, 1, 1)},
    {XYZ(1, -1, 1)},
    {XYZ(1, 1, -1)},
    {XYZ(1, -1, -1)},
    // back face
    {XYZ(-1, 1, 1)},
    {XYZ(1, 1, 1)},
    {XYZ(-1, -1, 1)},
    {XYZ(-1, -1, 1)},
    {XYZ(1, 1, 1)},
    {XYZ(1, -1, 1)},
};

typedef struct GPU {
	VkDevice device;
	VkPhysicalDevice physDevice;
	VkPhysicalDeviceProperties physProps;
	VkPhysicalDeviceMemoryProperties memProps;
	VkQueueFamilyProperties* queueProps;
	uint32_t gpuIdx, queues;
} GPU;

typedef struct Layer {
	VkLayerProperties properties;
	VkExtensionProperties* instanceExtensions;
} Layer;

// typedef struct PlatformHandle {
// #ifdef _WIN32
// 	HINSTANCE connection;
// 	HWND window;
// #elif defined(VK_USE_PLATFORM_MACOS_MVK)
// 	void* window;
// #else //linux
//     xcb_connection_t *connection;
//     xcb_screen_t *screen;
//     xcb_window_t window;
//     xcb_intern_atom_reply_t *atom_wm_delete_window;
// #endif
// } PlatformHandle;

typedef struct BackedBuffer {
	VkBuffer buffer;
	VkBufferCreateInfo bufferInfo;
	VkMemoryAllocateInfo memAlloc;
	VkMemoryRequirements memReqs;
	VkDeviceMemory mem;
	VkDescriptorBufferInfo bufferDescriptorInfo;
} BackedBuffer;

typedef struct BackedImageView {
	BackedBuffer buffer;
	VkImage image;
	VkImageView imageView;
	VkImageViewCreateInfo viewInfo;
	VkImageCreateInfo imageInfo;
	uint32_t width, height;
	VkMemoryAllocateInfo memAlloc;
	VkMemoryRequirements memReqs;
	VkDeviceMemory mem;
} BackedImageView;

typedef struct SampledImage {
	BackedImageView image;
	VkDescriptorImageInfo desc;
	VkSamplerCreateInfo samplerInfo;
} SampledImage;

uint32_t width, height;
VkExtent2D swapchainExtent;
char title[80];
// PlatformHandle platHandle;
GPU* gpu;
uint32_t graphicsQueueFamily, computeQueueFamily, presentQueueFamily;
VkSurfaceKHR surface;
uint32_t surfaceFormatCount;
VkSurfaceFormatKHR* surfaceFormats;
VkFormat surfaceFormat;
VkCommandPool cmdPool;
VkCommandBuffer cmd;
VkQueue graphics, present, compute;
VkSwapchainKHR swapchain;
uint32_t bufferCount, currentBuffer;
BackedImageView* buffers, depth;
BackedBuffer uniformDataT;
BackedBuffer uniformDataS;
VkDescriptorSetLayout descLayouts[2];
VkPipelineLayout pipelineLayout;
VkRenderPass renderPass;
VkPipelineShaderStageCreateInfo shaderStages[2];
VkFramebuffer *framebuffers;
BackedBuffer vertexBuffer;
VkVertexInputBindingDescription viBinding;
VkVertexInputAttributeDescription viAttrib;
VkDescriptorSet descSets[2];
VkDescriptorPool descPool;
VkPipelineCache pipelineCache;
VkPipeline pipeline;
VkViewport viewport;
VkRect2D scissors;
FILE* logfile;

//from util.cpp in vulkan sample. Debug only.
bool read_ppm(char const *const filename, uint32_t &width, uint32_t &height, uint64_t rowPitch, unsigned char *dataPtr) {
    // PPM format expected from http://netpbm.sourceforge.net/doc/ppm.html
    //  1. magic number
    //  2. whitespace
    //  3. width
    //  4. whitespace
    //  5. height
    //  6. whitespace
    //  7. max color value
    //  8. whitespace
    //  7. data
    // Comments are not supported, but are detected and we kick out
    // Only 8 bits per channel is supported
    // If dataPtr is nullptr, only width and height are returned
    // Read in values from the PPM file as characters to check for comments
    char magicStr[3] = {}, heightStr[6] = {}, widthStr[6] = {}, formatStr[6] = {};
#ifndef __ANDROID__
    FILE *fPtr = fopen(filename, "rb");
#else
    FILE *fPtr = AndroidFopen(filename, "rb");
#endif
    if (!fPtr) {
        printf("Bad filename in read_ppm: %s\n", filename);
        return false;
    }
    // Read the four values from file, accounting with any and all whitepace
    fscanf(fPtr, "%s %s %s %s ", magicStr, widthStr, heightStr, formatStr);
    // Kick out if comments present
    if (magicStr[0] == '#' || widthStr[0] == '#' || heightStr[0] == '#' || formatStr[0] == '#') {
        printf("Unhandled comment in PPM file\n");
        return false;
    }
    // Only one magic value is valid
    if (strncmp(magicStr, "P6", sizeof(magicStr))) {
        printf("Unhandled PPM magic number: %s\n", magicStr);
        return false;
    }
    width = atoi(widthStr);
    height = atoi(heightStr);
    // Ensure we got something sane for width/height
    static const int saneDimension = 32768;  //??
    if (width <= 0 || width > saneDimension) {
        printf("Width seems wrong.  Update read_ppm if not: %u\n", width);
        return false;
    }
    if (height <= 0 || height > saneDimension) {
        printf("Height seems wrong.  Update read_ppm if not: %u\n", height);
        return false;
    }
    if (dataPtr == nullptr) {
        // If no destination pointer, caller only wanted dimensions
        return true;
    }
    // Now read the data
    for (uint32_t y = 0; y < height; y++) {
        unsigned char *rowPtr = dataPtr;
        for (uint32_t x = 0; x < width; x++) {
            fread(rowPtr, 3, 1, fPtr);
            rowPtr[3] = 255; /* Alpha of 1 */
            rowPtr += 4;
        }
        dataPtr += rowPitch;
    }
    fclose(fPtr);
    return true;
}

uint32_t pickMemType(VkMemoryRequirements memReqs, GPU* gpu, VkFlags heapFlags, VkFlags typeFlags) {
	uint32_t ret = -1, i, j;
	for (i = 0, j = 1;j < memReqs.memoryTypeBits && ret == -1;i++, j = j << 1)
		if ((j & memReqs.memoryTypeBits) &&
			(gpu->memProps.memoryTypes[i].propertyFlags & typeFlags) == typeFlags && heapFlags ==
			(gpu->memProps.memoryHeaps[gpu->memProps.memoryTypes[i].heapIndex].flags & heapFlags) &&
			gpu->memProps.memoryHeaps[gpu->memProps.memoryTypes[i].heapIndex].size >= memReqs.size)
			ret = i;
	if (ret == -1) fprintf(logfile, "Warn: failed to find appropriate heap\n");
	return ret;
}

GPU *gpus;
uint32_t gpuCount;
Layer* instanceLayers;
VkInstance inst;
SDL_Window* sdlWindow;

#define FAIL { fprintf(logfile, "Fail. Res: %d. Line: %lu\n", res, (unsigned long)__LINE__); return 1;}

int main(int argc, char** argv) {
  VkResult res;
  uint32_t count, i, j, priority, k;
  uint64_t line = 0;
  void* tempP;
  VkBool32 tempBool;
  logfile = fopen("runtime.log", "w");
  //SampledImage tex = {};
  //process_command_line_args(info, argc, argv);
  //init_global_layer_properties(info);
  //TODO sync lock?
  if(inst == NULL) {
    VkLayerProperties *vk_props = NULL;
    VkResult res;
    do {
      res = vkEnumerateInstanceLayerProperties(&count, NULL);
      if (res) FAIL;
      if (count > 0) {
	vk_props = (VkLayerProperties *)realloc(vk_props,
						count * sizeof(VkLayerProperties));
	res = vkEnumerateInstanceLayerProperties(&count, vk_props);
      }
    } while (res == VK_INCOMPLETE);
    instanceLayers = (Layer*)malloc(count * sizeof(Layer));
    if (!instanceLayers) FAIL;
    for (uint32_t i = 0; i < count; i++) {
      instanceLayers[i].properties = vk_props[i];
      //init_global_extension_properties(layer_props);
      uint32_t instance_extension_count = 0;
      char *layer_name = NULL;
      layer_name = vk_props[i].layerName;
      do {
	res = vkEnumerateInstanceExtensionProperties(layer_name, &instance_extension_count, NULL);
	if (res) FAIL;
	if (instance_extension_count > 0) {
	  instanceLayers[i].instanceExtensions =
	    (VkExtensionProperties*)realloc(instanceLayers[i].instanceExtensions,
					    instance_extension_count * sizeof(VkExtensionProperties));
	  res = vkEnumerateInstanceExtensionProperties(layer_name,
						       &instance_extension_count, instanceLayers[i].instanceExtensions);
	}
      } while (res == VK_INCOMPLETE);
    }
    free(vk_props);
    //init_instance_extension_names(info);//static only
    //init_device_extension_names(info);//static only
    //init_instance(info, sample_title);
    std::unique_ptr<const char*[]> instanceExtensions;
    uint32_t instanceExtensionsCnt;
    {
      auto tempWin = SDL_CreateWindow("surface probe", 0, 0, 100, 100, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
      if(!SDL_Vulkan_GetInstanceExtensions(tempWin, &instanceExtensionsCnt, NULL))
	FAIL;
      instanceExtensions = std::make_unique<const char*[]>(instanceExtensionsCnt);
      if(!SDL_Vulkan_GetInstanceExtensions(tempWin, &instanceExtensionsCnt, instanceExtensions.get()))
	FAIL;
      SDL_DestroyWindow(tempWin);
    }
    const char* instanceLayers[] {
      "VK_LAYER_KHRONOS_validation"
    };
    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "Standalone Cube",
      0, "Example", 0, VK_API_VERSION_1_3 };
    VkInstanceCreateInfo instanceInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL,
      0, &appInfo, 1, instanceLayers, instanceExtensionsCnt, instanceExtensions.get() };
    res = vkCreateInstance(&instanceInfo, NULL, &inst);
    if (res) FAIL;
    //init_enumerate_device(info);
    res = vkEnumeratePhysicalDevices(inst, &gpuCount, NULL);
    if (res) FAIL;
    VkPhysicalDevice* phys = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gpuCount);
    gpus = (GPU*)malloc(sizeof(GPU) * gpuCount);
    if (!gpus) FAIL;
    res = vkEnumeratePhysicalDevices(inst, &gpuCount, phys);
    if (res) FAIL;
    for(i = 0;i < gpuCount;i++) {
      gpus[i].gpuIdx = i;
      gpus[i].physDevice = phys[i];
      vkGetPhysicalDeviceProperties(phys[i], &gpus[i].physProps);
      vkGetPhysicalDeviceMemoryProperties(phys[i], &gpus[i].memProps);
      vkGetPhysicalDeviceQueueFamilyProperties(phys[i], &gpus[i].queues, NULL);
      gpus[i].queueProps = (VkQueueFamilyProperties*)
	malloc(sizeof(VkQueueFamilyProperties) * gpus[i].queues);
      if (!gpus[i].queueProps) FAIL;
      vkGetPhysicalDeviceQueueFamilyProperties(phys[i], &gpus[i].queues, gpus[i].queueProps);
      gpus[i].device = NULL;
    }
    free(phys);
  }
  //init_window_size(info, 500, 500);
  width = 960;
  height = 540;
  //init_connection(info);
  //init_window(info);
  sprintf(title, "Vulkan window");
  sdlWindow = SDL_CreateWindow("WITE", 0, 0, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
  if(!sdlWindow || !SDL_Vulkan_CreateSurface(sdlWindow, inst, &surface))
    FAIL;
  priority = 0;
  // printf("GPUS:\n");
  gpu = NULL;
  for (i = 0;i < gpuCount;i++) {
    // std::cout << "GPU[" << i << "]:\n";
    if(!gpu) graphicsQueueFamily = computeQueueFamily = -1;
    for (j = 0;j < gpus[i].queues;j++) {
      vkGetPhysicalDeviceSurfaceSupportKHR(gpus[i].physDevice, j, surface, &tempBool);
      //tempBool = this surface works with this gpu and queue
      k = 0;
      if (tempBool == VK_TRUE) k |= 4;
      if (gpus[i].queueProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
	k |= 2;
	if (graphicsQueueFamily == -1) graphicsQueueFamily = j;
      }
      if (gpus[i].queueProps[j].queueFlags & VK_QUEUE_COMPUTE_BIT) {
	k |= 1;
	if (computeQueueFamily == -1) computeQueueFamily = j;
      }
      // printf("\t%I32u: %I32u :: priority: %I32u\n", j, gpus[i].queueProps[j].queueFlags, k);
      if (k > priority) {
	priority = k;
	if (k & 4) {
	  gpu = &gpus[i];
	  presentQueueFamily = j;
	}
	if (k & 2) graphicsQueueFamily = j;
	if (k & 1) computeQueueFamily = j;
	//TODO split gpu potential case: render on A present on B (laptops)
      }
    }
  }
  if (!gpu || graphicsQueueFamily == -1) {
    printf("Could not find a gpu that can present. Death by decapitation.");
    FAIL;
  }
  res = vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->physDevice, surface, &surfaceFormatCount, NULL);
  if (res) FAIL;
  surfaceFormats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * surfaceFormatCount);
  if (!surfaceFormats) FAIL;
  res = vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->physDevice, surface, &surfaceFormatCount,
					     surfaceFormats);
  if (res) FAIL;
  surfaceFormat = (surfaceFormats[0].format == VK_FORMAT_UNDEFINED) ?
    VK_FORMAT_B8G8R8A8_UNORM : surfaceFormats[0].format;//TODO be more picky?
  //init_device(info);
  if (!gpu->device) {
    const char* deviceExtensions[] = {
      "VK_KHR_swapchain"
    };
    float priorities = 0.0f;
    VkDeviceQueueCreateInfo queueInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      NULL, 0, graphicsQueueFamily, 1, &priorities};
    VkDeviceCreateInfo deviceInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, 1, &queueInfo,
      0, NULL, 1, deviceExtensions, NULL};
    res = vkCreateDevice(gpu->physDevice, &deviceInfo, NULL, &gpu->device);
    if (res) FAIL;
  }
  //init_command_pool(info); was here
  //init_command_buffer(info); waas here
  //execute_begin_command_buffer(info); was here
  //init_device_queue(info);
  vkGetDeviceQueue(gpu->device, graphicsQueueFamily, 0, &graphics);
  if (graphicsQueueFamily == presentQueueFamily) present = graphics;
  else vkGetDeviceQueue(gpu->device, presentQueueFamily, 0, &present);
  //init_swap_chain(info);
  {
    VkSurfaceCapabilitiesKHR surfCaps;
    res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu->physDevice, surface, &surfCaps);
    if (res) FAIL;
    // fprintf(logfile, "surface supported usage flags: %I32X\n", surfCaps.supportedUsageFlags);
    //res = vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->physDevice, surface, &count, NULL);
    //if (res) FAIL;
    //VkPresentModeKHR* modes = (VkPresentModeKHR*)malloc(count * sizeof(VkPresentModeKHR));
    //if(!modes) FAIL;
    //res = vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->physDevice, surface, &count, modes);
    //if (res) FAIL;
    //free(modes);//sample code mallocs populates and frees but doesn't use.
    if (surfCaps.currentExtent.width == 0xFFFFFFFF) {
      swapchainExtent.width = width < surfCaps.minImageExtent.width ? surfCaps.maxImageExtent.width :
				      width > surfCaps.maxImageExtent.width ? surfCaps.maxImageExtent.width : width;
      swapchainExtent.height = height < surfCaps.minImageExtent.height ?
					surfCaps.maxImageExtent.height : height > surfCaps.maxImageExtent.height ?
	surfCaps.maxImageExtent.height : height;
    } else
      swapchainExtent = surfCaps.currentExtent;
    count = surfCaps.minImageCount;
    VkSurfaceTransformFlagBitsKHR preTransform =
      (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ?
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfCaps.currentTransform;
    static const VkCompositeAlphaFlagBitsKHR preferred[] = { VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR};
    VkCompositeAlphaFlagBitsKHR target = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    for(i = 0;i < sizeof(VkCompositeAlphaFlagsKHR);i++)
      if (surfCaps.supportedCompositeAlpha & preferred[i]) {
	target = preferred[i];
	break;
      }
    VkSwapchainCreateInfoKHR info;
    if (graphicsQueueFamily == presentQueueFamily) {
      info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, NULL, 0, surface, count, surfaceFormat,
	VK_COLORSPACE_SRGB_NONLINEAR_KHR, swapchainExtent, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 0,
	NULL, preTransform, target, VK_PRESENT_MODE_FIFO_KHR, false, NULL };
    } else {
      uint32_t qfs[2] = { graphicsQueueFamily, presentQueueFamily };
      info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, NULL, 0, surface, count, surfaceFormat,
	VK_COLORSPACE_SRGB_NONLINEAR_KHR, swapchainExtent, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SHARING_MODE_CONCURRENT, 2,
	qfs, preTransform, target, VK_PRESENT_MODE_FIFO_KHR, false, NULL };//fifo = queued vsync
    }
    res = vkCreateSwapchainKHR(gpu->device, &info, NULL, &swapchain);
    if (res) FAIL;
    res = vkGetSwapchainImagesKHR(gpu->device, swapchain, &bufferCount, NULL);
    if (res) FAIL;
    buffers = (BackedImageView*)malloc(sizeof(BackedImageView) * bufferCount);
    VkImage* images = (VkImage*)malloc(sizeof(VkImage) * bufferCount);
    if (!buffers || !images) FAIL;
    res = vkGetSwapchainImagesKHR(gpu->device, swapchain, &bufferCount, images);
    if (res) FAIL;
    for (i = 0;i < bufferCount;i++) {
      buffers[i].viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, images[i],
	VK_IMAGE_VIEW_TYPE_2D, surfaceFormat,{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
      buffers[i].image = images[i];
      buffers[i].width = swapchainExtent.width;
      buffers[i].height = swapchainExtent.height;
      memset(&buffers[i].buffer, 0, sizeof(BackedBuffer));
      res = vkCreateImageView(gpu->device, &buffers[i].viewInfo, NULL, &buffers[i].imageView);
      if (res) FAIL;
    }
    free(images);
    currentBuffer = 0;
  }
  //init_depth_buffer(info);
  {
    const VkFormat depthFormat = VK_FORMAT_D16_UNORM;
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(gpu->physDevice, depthFormat, &props);
    depth.imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D,
      depthFormat, {swapchainExtent.width, swapchainExtent.height, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT,
      (props.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ?
      VK_IMAGE_TILING_LINEAR :
      (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ?
      VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
      VK_IMAGE_LAYOUT_UNDEFINED };
    res = vkCreateImage(gpu->device, &depth.imageInfo, NULL, &depth.image);
    if (res) FAIL;
    vkGetImageMemoryRequirements(gpu->device, depth.image, &depth.buffer.memReqs);
    depth.buffer.memAlloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, depth.buffer.memReqs.size,
      pickMemType(depth.buffer.memReqs, gpu, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, 0) };
    res = vkAllocateMemory(gpu->device, &depth.buffer.memAlloc, NULL, &depth.buffer.mem);
    if (res) FAIL;
    res = vkBindImageMemory(gpu->device, depth.image, depth.buffer.mem, 0);
    if (res) FAIL;
    depth.viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, depth.image,
      VK_IMAGE_VIEW_TYPE_2D, depthFormat,{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
			 VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } };
    res = vkCreateImageView(gpu->device, &depth.viewInfo, NULL, &depth.imageView);
    if (res) FAIL;
    depth.width = swapchainExtent.width;
    depth.height = swapchainExtent.height;
  }
  //init_texture(info);
  // {
  //   //|->init_image(info, texObj)
  //   uint32_t usages = VK_IMAGE_USAGE_SAMPLED_BIT;
  //   memset(&tex, 0, sizeof(SampledImage));
  //   if (!read_ppm("../../StupidCube/data/lunarg.ppm", tex.image.width, tex.image.height, 0, NULL)) FAIL;
  //   VkFormatProperties props;
  //   vkGetPhysicalDeviceFormatProperties(gpus->physDevice, VK_FORMAT_R8G8B8A8_UNORM, &props);
  //   bool staged = !(props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
  //   if (staged) {
  //     //init_buffer(info, tex)
  //     tex.image.buffer.bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
  // 	tex.image.width * tex.image.height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
  // 	VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
  //     res = vkCreateBuffer(gpu->device, &tex.image.buffer.bufferInfo, NULL, &tex.image.buffer.buffer);
  //     if (res) FAIL;
  //     vkGetBufferMemoryRequirements(gpu->device, tex.image.buffer.buffer, &tex.image.buffer.memReqs);
  //     tex.image.buffer.memAlloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
  // 	tex.image.buffer.memReqs.size, pickMemType(tex.image.buffer.memReqs, gpu, 0,
  // 						   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
  //     res = vkAllocateMemory(gpu->device, &tex.image.buffer.memAlloc, NULL, &tex.image.buffer.mem);
  //     if (res) FAIL;
  //     res = vkBindBufferMemory(gpu->device, tex.image.buffer.buffer, tex.image.buffer.mem, 0);
  //     if (res) FAIL;
  //     //end init_buffer
  //     usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  //   } else
  //     memset(&tex.image.buffer, 0, sizeof(BackedBuffer));
  //   tex.image.imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D,
  //     VK_FORMAT_R8G8B8A8_UNORM, {tex.image.width, tex.image.height, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT,
  //     staged ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR, usages, VK_SHARING_MODE_EXCLUSIVE,
  //     0, NULL, staged ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PREINITIALIZED };
  //   res = vkCreateImage(gpu->device, &tex.image.imageInfo, NULL, &tex.image.image);
  //   if (res) FAIL;
  //   vkGetImageMemoryRequirements(gpu->device, tex.image.image, &tex.image.memReqs);
  //   tex.image.memAlloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
  //     tex.image.memReqs.size, pickMemType(tex.image.memReqs, gpu, 0,
  // 					  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
  //   res = vkAllocateMemory(gpu->device, &tex.image.memAlloc, NULL, &tex.image.mem);
  //   if (res) FAIL;
  //   res = vkBindImageMemory(gpu->device, tex.image.image, tex.image.mem, 0);
  //   if (res) FAIL;
  //   //***queue/fence break apparently unnecessary
  //   //res = vkEndCommandBuffer(cmd);//util_init:1839; but seems wrong
  //   //if (res) FAIL;
  //   //VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
  //   //VkFence cmdFence;
  //   //vkCreateFence(gpu->device, &fenceInfo, NULL, &cmdFence);
  //   //VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &cmd, 0, NULL };
  //   //res = vkQueueSubmit(graphics, 1, &submitInfo, cmdFence);
  //   //if (res) FAIL;
  //   VkImageSubresource subres = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
  //   VkSubresourceLayout layout = {};
  //   void* data;
  //   if (!staged)
  //     vkGetImageSubresourceLayout(gpu->device, tex.image.image, &subres, &layout);
  //   /*do {
  //     res = vkWaitForFences(gpu->device, 1, &cmdFence, VK_TRUE, 1000000000);
  //     } while (res == VK_TIMEOUT);
  //     if (res) FAIL;
  //     vkDestroyFence(gpu->device, cmdFence, NULL);*/
  //   if (staged)
  //     res = vkMapMemory(gpu->device, tex.image.buffer.mem, 0, tex.image.buffer.bufferInfo.size,
  // 			0, &data);
  //   else
  //     res = vkMapMemory(gpu->device, tex.image.mem, 0, tex.image.memReqs.size, 0, &data);
  //   if (res) FAIL;
  //   if (!read_ppm("../../StupidCube/data/lunarg.ppm", tex.image.width, tex.image.height, staged ? (tex.image.width * 4) :
  // 		  layout.rowPitch, (unsigned char *)data))
  //     FAIL;
  //   vkUnmapMemory(gpu->device, staged ? tex.image.buffer.mem : tex.image.mem);
  //   //VkCommandBufferBeginInfo cmdBufInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL};
  //   //res = vkBeginCommandBuffer(cmd, &cmdBufInfo);
  //   //***end queue/fence changes
  //   //if (res) FAIL;
    //init_command_pool(info); was earlier
    {
      VkCommandPoolCreateInfo cmdPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
	VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphicsQueueFamily };
      res = vkCreateCommandPool(gpu->device, &cmdPoolInfo, NULL, &cmdPool);
      if (res) FAIL;
    }
    //init_command_buffer(info); was earlier
    {
      VkCommandBufferAllocateInfo cmdInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
	cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
      res = vkAllocateCommandBuffers(gpu->device, &cmdInfo, &cmd);
      if (res) FAIL;
    }
    //execute_begin_command_buffer(info); was earlier
    {
      VkCommandBufferBeginInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL };
      res = vkBeginCommandBuffer(cmd, &info);
      if (res) FAIL;
    }
  //   if (!staged) {
  //     tex.desc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  //     /*set_image_layout(info, texObj.image, VK_IMAGE_ASPECT_COLOR_BIT,
  // 	VK_IMAGE_LAYOUT_PREINITIALIZED, texObj.imageLayout, VK_PIPELINE_STAGE_HOST_BIT,
  // 	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);*/
  //     //VkAccessFlags src = VK_ACCESS_HOST_WRITE_BIT, dst = VK_ACCESS_SHADER_READ_BIT;
  //     VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
  // 	VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED,
  // 	tex.desc.imageLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, tex.image.image,
  // 	{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
  //     vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
  // 			   0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
  //   } else {
  //     /*set_image_layout(info, texObj.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
  // 	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
  // 	VK_PIPELINE_STAGE_TRANSFER_BIT);*/
  //     VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
  // 	0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
  // 	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
  // 	tex.image.image, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
  //     vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
  // 			   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
  //     VkBufferImageCopy copyRegion = { 0, tex.image.width, tex.image.height,
  // 				       { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },{ 0, 0, 0 },{ tex.image.width, tex.image.height, 0 } };
  //     vkCmdCopyBufferToImage(cmd, tex.image.buffer.buffer, tex.image.image,
  // 			     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
  //     tex.desc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  //     /*set_image_layout(info, texObj.image, VK_IMAGE_ASPECT_COLOR_BIT,
  // 	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texObj.imageLayout, VK_PIPELINE_STAGE_TRANSFER_BIT,
  // 	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);*/
  //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  //     vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
  // 			   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
  //   }
  //   tex.image.viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
  //     tex.image.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, { VK_COMPONENT_SWIZZLE_R,
  // 			     VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
  //     { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
  //   res = vkCreateImageView(gpu->device, &tex.image.viewInfo, NULL, &tex.image.imageView);
  //   if (res) FAIL;
  //   //|<end init_image
  //   //|->init_sampler(info, tex.sampler)
  //   tex.samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, NULL, 0, VK_FILTER_NEAREST,
  //     VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  //     VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  //     0.0, VK_FALSE, 1, VK_FALSE, VK_COMPARE_OP_NEVER, 0, 0, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
  //     VK_FALSE };
  //   res = vkCreateSampler(gpu->device, &tex.samplerInfo, NULL, &tex.desc.sampler);
  //   if (res) FAIL;
  //   //|<end init_sampler
  //   tex.desc.imageView = tex.image.imageView;
  //   tex.desc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  // }
  //init_uniform_buffer(info);
  {
    float fov = glm::radians(45.0f) * swapchainExtent.height / swapchainExtent.width;
    glm::dmat4 VP = glm::dmat4(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0.5, 1) * //clip
      glm::perspectiveFov<double>(glm::radians(45.0f), 16.0, 9.0, 0.1, 100.0) * //projection
      glm::lookAt(glm::dvec3(-15, 13, -10), glm::dvec3(0, 0, 0), glm::dvec3(0, -1, 0)); //view
    uniformDataT.bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, sizeof(VP),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
    res = vkCreateBuffer(gpu->device, &uniformDataT.bufferInfo, NULL, &uniformDataT.buffer);
    if (res) FAIL;
    vkGetBufferMemoryRequirements(gpu->device, uniformDataT.buffer, &uniformDataT.memReqs);
    uniformDataT.memAlloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, uniformDataT.memReqs.size,
      pickMemType(uniformDataT.memReqs, gpu, 0, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
    res = vkAllocateMemory(gpu->device, &uniformDataT.memAlloc, NULL, &uniformDataT.mem);
    if (res) FAIL;
    res = vkMapMemory(gpu->device, uniformDataT.mem, 0, uniformDataT.memReqs.size, 0, &tempP);
    if (res) FAIL;
    memcpy(tempP, &VP, sizeof(VP));
    vkUnmapMemory(gpu->device, uniformDataT.mem);
    res = vkBindBufferMemory(gpu->device, uniformDataT.buffer, uniformDataT.mem, 0);
    if (res) FAIL;
    uniformDataT.bufferDescriptorInfo = { uniformDataT.buffer, 0, sizeof(VP) };
  }
  {
    glm::dmat4 model = glm::mat4(1);
    uniformDataS.bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, sizeof(model),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
    res = vkCreateBuffer(gpu->device, &uniformDataS.bufferInfo, NULL, &uniformDataS.buffer);
    if (res) FAIL;
    vkGetBufferMemoryRequirements(gpu->device, uniformDataS.buffer, &uniformDataS.memReqs);
    uniformDataS.memAlloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, uniformDataS.memReqs.size,
      pickMemType(uniformDataS.memReqs, gpu, 0, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
    res = vkAllocateMemory(gpu->device, &uniformDataS.memAlloc, NULL, &uniformDataS.mem);
    if (res) FAIL;
    res = vkMapMemory(gpu->device, uniformDataS.mem, 0, uniformDataS.memReqs.size, 0, &tempP);
    if (res) FAIL;
    memcpy(tempP, &model, sizeof(model));
    vkUnmapMemory(gpu->device, uniformDataS.mem);
    res = vkBindBufferMemory(gpu->device, uniformDataS.buffer, uniformDataS.mem, 0);
    if (res) FAIL;
    uniformDataS.bufferDescriptorInfo = { uniformDataS.buffer, 0, sizeof(model) };
  }
  //init_descriptor_and_pipeline_layouts(info, true);
  {
    VkDescriptorSetLayoutBinding layoutBinding = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL};
    VkDescriptorSetLayoutCreateInfo descriptorLayout = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &layoutBinding };
    res = vkCreateDescriptorSetLayout(gpu->device, &descriptorLayout, NULL, &descLayouts[0]);
    if (res) FAIL;
    res = vkCreateDescriptorSetLayout(gpu->device, &descriptorLayout, NULL, &descLayouts[1]);
    if (res) FAIL;
    VkPipelineLayoutCreateInfo pPipelineCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      NULL, 0, 2, descLayouts, 0, NULL};
    res = vkCreatePipelineLayout(gpu->device, &pPipelineCreateInfo, NULL, &pipelineLayout);
    if (res) FAIL;
  }
  //init_renderpass(info, depthPresent);//depthPresent = true
  {
    VkAttachmentDescription attachments[2] = {
      {0, surfaceFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR },
      {0, depth.imageInfo.format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
       VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}//only if depthPresent
    };
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
      depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, NULL, 1, &colorReference,
      NULL, &depthReference, 0, NULL };
    VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 2, attachments,
      1, &subpass, 0, NULL};
    res = vkCreateRenderPass(gpu->device, &rpInfo, NULL, &renderPass);
    if (res) FAIL;
  }
  //init_shaders(info, vertShaderText, fragShaderText);
  {
    //|-init_glslang (empty function on everything that's not android)
    shaderStages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
      VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE, "main", NULL};
    VkShaderModuleCreateInfo moduleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0,
      0, NULL};
    moduleCreateInfo.pCode = basicShader_vert;
    moduleCreateInfo.codeSize = sizeof(basicShader_vert);
    if(!moduleCreateInfo.pCode) FAIL;
    res = vkCreateShaderModule(gpu->device, &moduleCreateInfo, NULL, &shaderStages[0].module);
    // free((void*)moduleCreateInfo.pCode);
    if (res) FAIL;
    shaderStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
      VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE, "main", NULL };
    moduleCreateInfo.pCode = basicShader_frag;
    moduleCreateInfo.codeSize = sizeof(basicShader_frag);
    if(!moduleCreateInfo.pCode) FAIL;
    res = vkCreateShaderModule(gpu->device, &moduleCreateInfo, NULL, &shaderStages[1].module);
    // free((void*)moduleCreateInfo.pCode);
    if (res) FAIL;
  }
  //init_framebuffers(info, depthPresent);
  {
    VkImageView attachments[2] = { NULL, depth.imageView };
    VkFramebufferCreateInfo fbInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, renderPass,
      2, attachments, swapchainExtent.width, swapchainExtent.height, 1 };
    framebuffers = (VkFramebuffer*)malloc(bufferCount * sizeof(VkFramebuffer));
    if (!framebuffers) FAIL;
    for (i = 0;i < bufferCount;i++) {
      attachments[0] = buffers[i].imageView;
      res = vkCreateFramebuffer(gpu->device, &fbInfo, NULL, &framebuffers[i]);
      if (res) FAIL;
    }
  }
  //init_vertex_buffer(info, g_vb_texture_Data, sizeof(g_vb_texture_Data), sizeof(g_vb_texture_Data[0]), true);
  {
    vertexBuffer.bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
      sizeof(g_vb_texture_Data), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE,
      0, NULL };
    res = vkCreateBuffer(gpu->device, &vertexBuffer.bufferInfo, NULL, &vertexBuffer.buffer);
    if (res) FAIL;
    vkGetBufferMemoryRequirements(gpu->device, vertexBuffer.buffer, &vertexBuffer.memReqs);
    vertexBuffer.memAlloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, vertexBuffer.memReqs.size,
      pickMemType(vertexBuffer.memReqs, gpu, 0,
		  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
    res = vkAllocateMemory(gpu->device, &vertexBuffer.memAlloc, NULL, &vertexBuffer.mem);
    if (res) FAIL;
    vertexBuffer.bufferDescriptorInfo = { vertexBuffer.buffer, 0, vertexBuffer.memReqs.size };
    res = vkMapMemory(gpu->device, vertexBuffer.mem, 0, vertexBuffer.memReqs.size, 0, &tempP);
    if (res) FAIL;
    memcpy(tempP, g_vb_texture_Data, sizeof(g_vb_texture_Data));
    vkUnmapMemory(gpu->device, vertexBuffer.mem);
    res = vkBindBufferMemory(gpu->device, vertexBuffer.buffer, vertexBuffer.mem, 0);
    if (res) FAIL;
    viBinding = { 0, sizeof(g_vb_texture_Data[0]), VK_VERTEX_INPUT_RATE_VERTEX };
    viAttrib = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
  }
  //init_descriptor_pool(info, true);
  {
    VkDescriptorPoolSize typeCount = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      NULL, 0, 2, 1, &typeCount };
    res = vkCreateDescriptorPool(gpu->device, &descriptorPoolInfo, NULL, &descPool);
    if (res) FAIL;
  }
  //init_descriptor_set(info, true);
  {//NUM_DESCRIPTOR_SETS=1
    VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, NULL,
      descPool, 2, descLayouts };
    res = vkAllocateDescriptorSets(gpu->device, &allocInfo, descSets);
    if (res) FAIL;
    // VkWriteDescriptorSet writes[2] = {
    //   {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, descSets[0], 0, 0, 1,
    //    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &uniformDataS.bufferDescriptorInfo, NULL},
    //   {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, descSets[1], 0, 0, 1,
    //    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &uniformDataT.bufferDescriptorInfo, NULL}
    // };
    // vkUpdateDescriptorSets(gpu->device, 2, writes, 0, NULL);
    VkWriteDescriptorSet write
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, descSets[0], 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &uniformDataS.bufferDescriptorInfo, NULL};
    vkUpdateDescriptorSets(gpu->device, 1, &write, 0, NULL);
    write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, descSets[1], 0, 0, 1,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &uniformDataT.bufferDescriptorInfo, NULL};
    vkUpdateDescriptorSets(gpu->device, 1, &write, 0, NULL);
  }
  //init_pipeline_cache(info);
  {
    VkPipelineCacheCreateInfo pipelineCacheInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
      NULL, 0, 0, NULL };
    res = vkCreatePipelineCache(gpu->device, &pipelineCacheInfo, NULL, &pipelineCache);
    if (res) FAIL;
  }
  // {//debug dump of shaderStages
  //   for(size_t i = 0;i < sizeof(VkPipelineShaderStageCreateInfo)*2;i++)
  //     fprintf(logfile, "%02X ", ((unsigned char*)&shaderStages)[i]);
  //   for(size_t i = 0;i < 2;i++) {
  //     fprintf(logfile, "\n\n%d:\nsType=%I32d\npNext=%I64X\nflags=%I32X\nstage=%I32X\nmoduleHandle=%I64X\npName=%s\n", i,
  // 	      shaderStages[i].sType, shaderStages[i].pNext, shaderStages[i].flags, shaderStages[i].stage, shaderStages[i].module, shaderStages[i].pName);
  //   }
  //   fflush(logfile);
  // }
  //init_pipeline(info, depthPresent);
  {
    VkDynamicState dynamicStateEnables[2];
    memset(dynamicStateEnables, 0, sizeof(dynamicStateEnables));
    dynamicStateEnables[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamicStateEnables[1] = VK_DYNAMIC_STATE_SCISSOR;
    VkPipelineDynamicStateCreateInfo dynamicState = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, NULL, 0, 2, dynamicStateEnables };
    VkPipelineVertexInputStateCreateInfo vi = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0, 1, &viBinding, 1, &viAttrib };
    VkPipelineInputAssemblyStateCreateInfo ia = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };
    VkPipelineRasterizationStateCreateInfo rs = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL, 0, VK_FALSE, VK_FALSE,
      VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f };
    VkPipelineColorBlendAttachmentState attState = { 0, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
      VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO , VK_BLEND_FACTOR_ZERO , VK_BLEND_OP_ADD, 0xF };
    VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      NULL, 0, 0, VK_LOGIC_OP_NO_OP, 1, &attState, {1, 1, 1, 1} };
    VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      NULL, 0, 1, NULL, 1, NULL };
    VkStencilOpState stencilOp = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
      VK_COMPARE_OP_ALWAYS, 0, 0, 0 };
    VkPipelineDepthStencilStateCreateInfo ds = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL, 0, 1, 1,
      VK_COMPARE_OP_LESS_OR_EQUAL, 0, 0, stencilOp, stencilOp, 0, 0 };
    VkPipelineMultisampleStateCreateInfo ms = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL, 0, VK_SAMPLE_COUNT_1_BIT, 0, 0,
      NULL, 0, 0 };
    VkGraphicsPipelineCreateInfo pipeInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0,
      2, shaderStages, &vi, &ia, NULL, &vp, &rs, &ms, &ds, &cb, &dynamicState, pipelineLayout,
      renderPass, 0, NULL, 0 };
    res = vkCreateGraphicsPipelines(gpu->device, pipelineCache, 1, &pipeInfo, NULL, &pipeline);
    if (res) FAIL;
  }
  /* VULKAN_KEY_START */

  VkClearValue clear_values[2];
  clear_values[0].color.float32[0] = 0.2f;
  clear_values[0].color.float32[1] = 0.2f;
  clear_values[0].color.float32[2] = 0.2f;
  clear_values[0].color.float32[3] = 0.2f;
  clear_values[1].depthStencil.depth = 1.0f;
  clear_values[1].depthStencil.stencil = 0;

  VkSemaphore imageAcquiredSemaphore;
  VkSemaphoreCreateInfo imageAcquiredSemaphoreCreateInfo;
  imageAcquiredSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  imageAcquiredSemaphoreCreateInfo.pNext = NULL;
  imageAcquiredSemaphoreCreateInfo.flags = 0;

  res = vkCreateSemaphore(gpu->device, &imageAcquiredSemaphoreCreateInfo, NULL, &imageAcquiredSemaphore);
  assert(res == VK_SUCCESS);

  // Get the index of the next available swapchain image:
  res = vkAcquireNextImageKHR(gpu->device, swapchain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE,
			      &currentBuffer);
  // TODO: Deal with the VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
  // return codes
  assert(res == VK_SUCCESS);

  VkRenderPassBeginInfo rp_begin;
  rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp_begin.pNext = NULL;
  rp_begin.renderPass = renderPass;
  rp_begin.framebuffer = framebuffers[currentBuffer];
  rp_begin.renderArea.offset.x = 0;
  rp_begin.renderArea.offset.y = 0;
  rp_begin.renderArea.extent.width = width;
  rp_begin.renderArea.extent.height = height;
  rp_begin.clearValueCount = 2;
  rp_begin.pClearValues = clear_values;
  //init_viewports(info);
  {
    viewport = {0, 0, (float)width, (float)height, 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
  }
  //init_scissors(info);
  {
    scissors = {{},{width,height}};
    vkCmdSetScissor(cmd, 0, 1, &scissors);
  }
  vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, descSets, 0, NULL);

  const VkDeviceSize offsets[1] = {0};
  vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, offsets);
  // viewport and scissors were here
  vkCmdDraw(cmd, 12 * 3, 1, 0, 0);
  vkCmdEndRenderPass(cmd);
  res = vkEndCommandBuffer(cmd);
  assert(res == VK_SUCCESS);

  const VkCommandBuffer cmd_bufs[] = {cmd};
  VkFenceCreateInfo fenceInfo;
  VkFence drawFence;
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.pNext = NULL;
  fenceInfo.flags = 0;
  vkCreateFence(gpu->device, &fenceInfo, NULL, &drawFence);

  VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info[1] = {};
  submit_info[0].pNext = NULL;
  submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info[0].waitSemaphoreCount = 1;
  submit_info[0].pWaitSemaphores = &imageAcquiredSemaphore;
  submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
  submit_info[0].commandBufferCount = 1;
  submit_info[0].pCommandBuffers = cmd_bufs;
  submit_info[0].signalSemaphoreCount = 0;
  submit_info[0].pSignalSemaphores = NULL;

  /* Queue the command buffer for execution */
  res = vkQueueSubmit(graphics, 1, submit_info, drawFence);
  assert(res == VK_SUCCESS);

  /* Now present the image in the window */

  VkPresentInfoKHR presentInfo;
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = NULL;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain;
  presentInfo.pImageIndices = &currentBuffer;
  presentInfo.pWaitSemaphores = NULL;
  presentInfo.waitSemaphoreCount = 0;
  presentInfo.pResults = NULL;

  /* Make sure command buffer is finished before presenting */
  do {
    res = vkWaitForFences(gpu->device, 1, &drawFence, VK_TRUE, 1000000);
  } while (res == VK_TIMEOUT);
  assert(res == VK_SUCCESS);
  res = vkQueuePresentKHR(present, &presentInfo);
  assert(res == VK_SUCCESS);

  sleep(3);
  /* VULKAN_KEY_END */
  //if (info.save_images) write_ppm(info, "draw_textured_cube");
  /*
    vkDestroyFence(info.device, drawFence, NULL);
    vkDestroySemaphore(info.device, imageAcquiredSemaphore, NULL);
    destroy_pipeline(info);
    destroy_pipeline_cache(info);
    destroy_textures(info);
    destroy_descriptor_pool(info);
    destroy_vertex_buffer(info);
    destroy_framebuffers(info);
    destroy_shaders(info);
    destroy_renderpass(info);
    destroy_descriptor_and_pipeline_layouts(info);
    destroy_uniform_buffer(info);
    destroy_depth_buffer(info);
    destroy_swap_chain(info);
    destroy_command_buffer(info);
    destroy_command_pool(info);
    destroy_device(info);
    destroy_window(info);
    destroy_instance(info);*/
  return 0;
}


