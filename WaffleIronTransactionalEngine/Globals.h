#pragma once

#include "stdafx.h"
#include "exportTypes.h"
#include "RenderLayer.h"

#define ADDED_EXTENSION_COUNT (0)
#define MAX_SDL_EXTENSIONS (20)
#define MAX_EXTENSIONS (MAX_SDL_EXTENSIONS+ADDED_EXTENSION_COUNT)
#define MAX_GPUS (32)
#define MAX_QUEUES (32)
#define DEVICE_EXTENSIONS_LEN (1)

#if defined(_DEBUG)
#define LAYER_COUNT 1
#define LAYERS { "VK_LAYER_LUNARG_standard_validation" }
#else
#define LAYER_COUNT 0
#define LAYERS {}
#endif

typedef struct {
  const char* extensions[MAX_EXTENSIONS];
  unsigned extensionCount, layerCount;
  const char* layers[LAYER_COUNT] = LAYERS;
  VkApplicationInfo appInfo;
  VkInstanceCreateInfo createInfo;
  VkInstance instance = NULL;
  WITE::SyncLock lock;
  unsigned gpuCount;
  class GPU* gpus[MAX_GPUS];
  class RenderLayer renderLayers[MAX_RENDER_LAYERS];
  uint64_t vramGrabSize = (6ull*1024*1024*1024/SIZEOF_VERTEX);//TODO dynamic; total in verts, divide by Mesh.VERTEX_BUFFERS for per-buffer size
} vulkan_singleton_t;

#define vkSingleton (*get_vkSingleton())
extern vulkan_singleton_t* get_vkSingleton();
extern const char** ADDED_EXTENSIONS;
extern const char* DEVICE_EXTENSIONS[DEVICE_EXTENSIONS_LEN];
#define vkAlloc VK_NULL_HANDLE
size_t getThreadCount();
uint8_t getVertBuffer();

//helpers: (different header?)
#ifdef _WIN32
#define CLAMPTODWORD(val) ((DWORD)glm::min<uint64_t>(val, UINT32_MAX))
#endif
