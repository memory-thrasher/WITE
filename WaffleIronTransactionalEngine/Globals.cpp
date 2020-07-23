#include "stdafx.h"
#include "GPU.h"
#include "Globals.h"
#include "Thread.h"
#include "Database.h"


#include <sys/stat.h>

FILE* errLogFile;
const char** ADDED_EXTENSIONS;
const char* DEVICE_EXTENSIONS[DEVICE_EXTENSIONS_LEN] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
//const VkAllocationCallbacks *vkAlloc = NULL;//now a define in Globals.h; TODO like errno and errLogFile
vulkan_singleton_t* get_vkSingleton() {
	static vulkan_singleton_t vulkankSingleton;
	return &vulkankSingleton;
}

namespace WITE {

#ifdef _WIN32

  export_def uint64_t timeNs() {
    FILETIME ft;
	ULARGE_INTEGER ret;
    GetSystemTimeAsFileTime(&ft);
	ret.LowPart = ft.dwLowDateTime;
	ret.HighPart = ft.dwHighDateTime;
    return ret.QuadPart * 100;
    //wraparound is ~June 2185;  64 bit uint in nanoseconds
	/*/
	uint64_t ret;
	QueryUnbiasedInterruptTime(&ret);
	return ret;//*/
  }//this is broken

  export_def void sleep(uint64_t ns) {
    uint64_t now = timeNs(), end = now + ns;
    while (now < end) {
      Sleep(CLAMPTODWORD((end - now) / NS_PER_MS));
      now = timeNs();
    }
  }

#endif

  export_def FILE* file_open(const char* path, const char* mode) {
    FILE* ret = fopen(path, mode);
    if (!ret)
      exit(2);
    return ret;
  }

  export_def void flush(FILE* file) {
    fflush(file);
  }

  export_def FILE* getERRLOGFILE() {
    return errLogFile;
  }

  export_def void WITE_INIT(const char* gameName) {//TODO move this to mainLoop?
    unsigned int extensionCount;
    const char* extensions[MAX_EXTENSIONS];
    WITE::Thread::init();
    VkPhysicalDevice devices[MAX_GPUS];
    size_t i;
    errLogFile = file_open("out.log", "w");
    SDL_Window* window = SDL_CreateWindow("splash", 0, 0, 10, 10, SDL_WINDOW_VULKAN);
    //window is a dumb requirement, deprecated as of SDL 2.0.8, will be dropped
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, NULL)) CRASH("Failed to fetch VK extension count needed by gl\n");
    if (extensionCount > MAX_SDL_EXTENSIONS) CRASH("Too many extensions\n");
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions)) CRASH("Failed to fetch VK extensions needed by gl\n");
    SDL_DestroyWindow(window);
    for (i = 0;i < ADDED_EXTENSION_COUNT;i++)
      extensions[extensionCount++] = ADDED_EXTENSIONS[i];
    vkSingleton.extensionCount = extensionCount;
    memcpy(vkSingleton.extensions, extensions, sizeof(char*) * extensionCount);
    vkSingleton.appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, gameName,
			    0, "Waffle Iron Transactional Engine (WITE)", 0, VK_API_VERSION_1_1 };
    vkSingleton.createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL,
			       0, &vkSingleton.appInfo, 0, NULL, vkSingleton.extensionCount, vkSingleton.extensions };
    CRASHIFFAIL(vkCreateInstance(&vkSingleton.createInfo, vkAlloc, &vkSingleton.instance));
    CRASHIFFAIL(vkEnumeratePhysicalDevices(vkSingleton.instance, &vkSingleton.gpuCount, NULL));
    if (vkSingleton.gpuCount > MAX_GPUS) vkSingleton.gpuCount = MAX_GPUS;//unlikely
    CRASHIFFAIL(vkEnumeratePhysicalDevices(vkSingleton.instance, &vkSingleton.gpuCount, devices));
    for (i = 0;i < vkSingleton.gpuCount;i++) {
      vkSingleton.gpus[i] = new GPU(devices[i], i);
    }
    for (i = 0;i < MAX_RENDER_LAYERS;i++) vkSingleton.renderLayers[i].idx = i;
  }

  export_def size_t getFileSize(const char* filename) {
	  struct stat fstat;
	  if (stat(filename, &fstat))
		  return NULL;
	  return fstat.st_size;
  }

  export_def bool loadFile(const char* filename, size_t &lenout, const unsigned char** dataout) {
    struct stat fstat;
    size_t len, offset = 0, read;
    if (stat(filename, &fstat))
      return NULL;
    lenout = len = fstat.st_size;
    *dataout = (unsigned char *)malloc((1 + len / 4) * 4 + 1);
    //padded to 32 bit barrier to accomodate unsigned int type
    if (!*dataout) return false;
    FILE* src = file_open(filename, "rb");
    if (!src) goto fail;
    read = fread((void*)(*dataout), 1, len, src);
    while (read > 0 && len >= 0) {
      len -= read;
      offset += read;
      read = fread((char*)(*dataout) + offset, 1, len, src);
    }
    //dataout[offset] = 0;//null term, probably unneeded
    fclose(src);
    if (lenout != offset) goto fail;
    return true;
  fail:
    LOG("Failed to read file: %s\n", filename);
    free((void*)(*dataout));
    *dataout = NULL;
    return false;
  }

  export_def bool endsWith(const char* str, const char* suffix) {
    return strlen(str) >= strlen(suffix) && 0 == strcmp(str + strlen(str) - strlen(suffix), suffix);
  }

  export_def bool startsWith(const char* str, const char* prefix) {
    return 0 == strncmp(str, prefix, strlen(prefix));
  }

  export_def bool strcmp_s(const std::string& a, const std::string& b) {
    return a.compare(b) < 0;
  }

  export_def WITE::Database** get_database() {
	  static WITE::Database* _database;
	  return &_database;
  }
  
}
