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

struct vulkan_singleton {
  const char* extensions[MAX_EXTENSIONS];
  unsigned extensionCount, layerCount;
  const char* layers[LAYER_COUNT];
  VkApplicationInfo appInfo;
  VkInstanceCreateInfo createInfo;
  VkInstance instance = NULL;
  WITE::SyncLock lock;
  unsigned gpuCount;
  class GPU* gpus[MAX_GPUS];
  class RenderLayer renderLayers[MAX_RENDER_LAYERS];
  uint64_t vramGrabSize = (6ull*1024*1024*1024/SIZEOF_VERTEX);//TODO dynamic; total in verts, divide by Mesh.VERTEX_BUFFERS for per-buffer size
};

extern struct vulkan_singleton vkSingleton;
extern const char** ADDED_EXTENSIONS;
extern const char* DEVICE_EXTENSIONS[DEVICE_EXTENSIONS_LEN];
extern const VkAllocationCallbacks *vkAlloc;
extern size_t threadCount;
extern WITE::Database* database;
extern uint8_t vertBuffer;

//helpers: (different header?)
#ifdef _WIN32
#define CLAMPTODWORD(val) ((DWORD)glm::min<uint64_t>(val, UINT32_MAX))
#endif
