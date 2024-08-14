/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <vector>
#include <map>
#include <bit>
#include <cstring>
#include <cstdlib>

#include "gpu.hpp"
#include "bitmaskIterator.hpp"
#include "math.hpp"
#include "DEBUG.hpp"
#include "window.hpp"
#include "configuration.hpp"

const std::map<vk::MemoryPropertyFlags, int64_t> memoryScoreByFlag = {
  { vk::MemoryPropertyFlagBits::eDeviceLocal      ,   256 },
  { vk::MemoryPropertyFlagBits::eHostVisible      ,   -64 },
  { vk::MemoryPropertyFlagBits::eHostCoherent     ,     8 },
  { vk::MemoryPropertyFlagBits::eHostCached       ,    16 },
  { vk::MemoryPropertyFlagBits::eLazilyAllocated  ,  -128 },
  { vk::MemoryPropertyFlagBits::eProtected        ,     0 },
  { vk::MemoryPropertyFlagBits::eDeviceCoherentAMD,   256 },
  { vk::MemoryPropertyFlagBits::eDeviceUncachedAMD,   -32 }
};

namespace WITE {

  //statics
  std::atomic_bool gpu::inited = false;
  vk::Instance gpu::vkInstance;
  size_t gpu::gpuCount;
  std::unique_ptr<gpu[]> gpu::gpus;
  char gpu::appName[1024];
  bool gpu::running;

  gpu::gpu() : tempCmds(NULL) {};//init dummy

  gpu::gpu(size_t idx, vk::PhysicalDevice pv, int deviceExtensionsCount, const char** deviceExtensions) :
    idx(idx),
    pv(pv),
    tempCmds(decltype(tempCmds)::Initer_F::make<size_t>(idx, &cmdPool::lowPrioForDevice))
  {
    pv.getProperties2(&pvp);
    uint32_t cnt = 0;
    pv.getQueueFamilyProperties2(&cnt, NULL);
    {
      std::unique_ptr<vk::QueueFamilyProperties2[]> temp_qfp = std::make_unique<vk::QueueFamilyProperties2[]>(cnt);
      qfp = std::make_unique<qfp_t[]>(cnt);
      for(size_t i = 0;i < cnt;i++)
	temp_qfp[i].pNext = reinterpret_cast<void*>(&qfp[i].gpp);
      pv.getQueueFamilyProperties2(&cnt, temp_qfp.get());
      for(size_t i = 0;i < cnt;i++)
	qfp[i].p = temp_qfp[i];
      //delete temp_qfp
    }
    queueIdx = ~0;
    auto allTypes = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer;
    for(size_t i = 0;i < cnt;i++) {
      if((qfp[i].p.queueFamilyProperties.queueFlags & allTypes) == allTypes) {
	queueIdx = i;
	break;
      }
    }
    float priorities[2] = { 0, 1 };
    uint8_t qCount = min(qfp[queueIdx].p.queueFamilyProperties.queueCount, 2);
    vk::DeviceQueueCreateInfo dqci { {}, queueIdx, qCount, priorities };

    vk::PhysicalDeviceFeatures pdf;
    pdf.geometryShader = true;
    // pdf.tessellationShader = true;
    // pdf.logicOp = true;
    // pdf.vertexPipelineStoresAndAtomics = true;
    // pdf.fragmentStoresAndAtomics = true;
    // pdf.shaderStorageImageMultisample = true;
    pdf.shaderClipDistance = true;
    pdf.shaderCullDistance = true;
    pdf.shaderFloat64 = true;
    pdf.shaderInt64 = true;
    // pdf.shaderInt16 = true;
    vk::PhysicalDeviceVulkan11Features pdf11;
    // pdf11.storageBuffer16BitAccess = true;
    // pdf11.uniformAndStorageBuffer16BitAccess = true;
    // pdf11.storagePushConstant16 = true;
    // pdf11.multiview = true;
    // pdf11.multiviewGeometryShader = true;
    // pdf11.multiviewTessellationShader = true;
    vk::PhysicalDeviceVulkan12Features pdf12;
    pdf11.pNext = reinterpret_cast<void*>(&pdf12);
    // pdf12.storageBuffer8BitAccess = true;
    // pdf12.uniformAndStorageBuffer8BitAccess = true;
    // pdf12.storagePushConstant8 = true;
    // pdf12.shaderBufferInt64Atomics = true;
    // pdf12.shaderSharedInt64Atomics = true;
    // pdf12.shaderInt8 = true;
    pdf12.timelineSemaphore = true;
    pdf12.drawIndirectCount = true;
    vk::PhysicalDeviceVulkan13Features pdf13;
    pdf12.pNext = reinterpret_cast<void*>(&pdf13);
    pdf13.synchronization2 = true;
    //begin mesh
    bool containsMesh = false;
    for(size_t i = 0;i < deviceExtensionsCount && !containsMesh;i++)
      if(!std::strcmp(deviceExtensions[i], "VK_EXT_mesh_shader"))
	containsMesh = true;
    vk::PhysicalDeviceMeshShaderFeaturesEXT pdfMesh;
    if(containsMesh) [[likely]] {
      pdfMesh.taskShader = pdfMesh.meshShader = true;
      pdf13.pNext = reinterpret_cast<void*>(&pdfMesh);
      pdf13.maintenance4 = true;//mesh shaders need LocalSizeId apparently
    }
    //end mesh
    vk::DeviceCreateInfo dci((vk::DeviceCreateFlags)0, 1, &dqci);
    dci.pEnabledFeatures = &pdf;
    dci.pNext = reinterpret_cast<void*>(&pdf11);
    // deviceExtensions.push_back("VK_KHR_synchronization2");
    dci.enabledExtensionCount = deviceExtensionsCount;
    dci.ppEnabledExtensionNames = deviceExtensions;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &dqci;
    VK_ASSERT(pv.createDevice(&dci, ALLOCCB, &dev), "Failed to create device");
    queue = dev.getQueue(queueIdx, qCount-1);
    lowPrioQueue = dev.getQueue(queueIdx, 0);
    ASSERT_TRAP(lowPrioQueue, "Device created but no lp queue seems to exist.");
    ASSERT_TRAP(queue, "Device created but no queue seems to exist.");

    for(auto format : Format::standardFormats)
      pv.getFormatProperties(format, &formatProperties[format]);

    //TODO look into save/load the cache (create info takes pointer to loaded data))
    vk::PipelineCacheCreateInfo pipecacheci((vk::PipelineCacheCreateFlags)0, 0, NULL);
    VK_ASSERT(dev.createPipelineCache(&pipecacheci, ALLOCCB, &pipelineCache), "failed to create pipeline cache");

    pdmp.pNext = &pdmbp;
    pv.getMemoryProperties2(&pdmp);
    for(size_t i = 0;i < pdmp.memoryProperties.memoryTypeCount;i++) {
      int64_t score = 0;
      for(auto pair : memoryScoreByFlag)
	if(pdmp.memoryProperties.memoryTypes[i].propertyFlags & pair.first)
	  score += pair.second;
      memoryScoreByType[i] = score;
      memoryFlagsByType[i] = pdmp.memoryProperties.memoryTypes[i].propertyFlags;
    }
    for(size_t i = 0;i < pdmp.memoryProperties.memoryHeapCount;i++)
      freeMemoryByHeap[i] = pdmbp.heapBudget[i] - pdmbp.heapUsage[i];
  };

  void gpu::allocate(const vk::MemoryRequirements& mr, vk::MemoryPropertyFlags requiredFlags, vram* out) {
    uint8_t type;
    int64_t bestScore = std::numeric_limits<int64_t>::lowest();
    for(auto t : bitmaskIterator(mr.memoryTypeBits)) {
      if(memoryScoreByType[t] > bestScore &&
	 freeMemoryByHeap[pdmp.memoryProperties.memoryTypes[t].heapIndex] > mr.size &&
	 (memoryFlagsByType[t] & requiredFlags) == requiredFlags) {
	bestScore = memoryScoreByType[t];
	type = t;
      }
    }
    //TODO care about (free) space. Multiply score by free memory in mb? Track free memory in an atomic per type
    new(out)vram(mr, type, this);
    freeMemoryByHeap[pdmp.memoryProperties.memoryTypes[type].heapIndex] -= mr.size;
  };

  void gpu::recordDeallocate(vram* doom) {
    freeMemoryByHeap[pdmp.memoryProperties.memoryTypes[doom->mai.memoryTypeIndex].heapIndex] += doom->mai.allocationSize;
  };

  gpu::vram::vram(const vk::MemoryRequirements& mr, uint8_t type, gpu* dev) : mai(mr.size, type), dev(dev) {
    VK_ASSERT(dev->getVkDevice().allocateMemory(&mai, ALLOCCB, &handle), "failed to allocate vram");
  };

  gpu::vram::vram(gpu::vram&& o) : handle(o.handle), mai(o.mai), dev(o.dev) {
    o.handle = VK_NULL_HANDLE;
  };

  gpu::vram::~vram() {
    if(!dev || !handle) return;
    free();
  };

  void gpu::vram::free() {
    dev->recordDeallocate(this);
    dev->getVkDevice().freeMemory(handle, ALLOCCB);
    dev = NULL;
  };

  void gpu::shutdown() {
    running = false;
  };

  void gpu::init(const char* appName,
		 std::initializer_list<const char*> appRequestedLayers,
		 std::initializer_list<const char*> appRequestedExtensions,
		 std::initializer_list<const char*> appRequestedDeviceExtensions) {//static
    if(inited.exchange(true))
      return;
    running = true;
    vk::ApplicationInfo appI(appName, 0, "WITE Engine", 0, (1 << 22) | (3 << 12) | (216));
    strcpy(gpu::appName, appName);
    vk::InstanceCreateInfo ci((vk::InstanceCreateFlags)0, &appI);
    std::vector<const char*> extensions(appRequestedExtensions);
    std::vector<const char*> layers(appRequestedLayers);
    std::vector<const char*> deviceExtensions(appRequestedDeviceExtensions);
#ifdef DEBUG
    layers.push_back("VK_LAYER_KHRONOS_validation");
    //layers.push_back("VK_LAYER_LUNARG_api_dump");//better to enable selectively with VK_INSTANCE_LAYERS env var
#endif
    window::addInstanceExtensionsTo(extensions);
    deviceExtensions.push_back("VK_KHR_swapchain");
    for(const char* e : extensions)
      LOG("Using extension: ", e);
    for(const char* l : layers)
      LOG("Using layer: ", l);
    for(const char* d : deviceExtensions)
      LOG("Using device extension: ", d);
    ci.enabledLayerCount = layers.size();
    ci.ppEnabledLayerNames = layers.data();
    ci.enabledExtensionCount = extensions.size();
    ci.ppEnabledExtensionNames = extensions.data();
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
    VK_ASSERT(vk::createInstance(&ci, ALLOCCB, &vkInstance), "Failed to create vulkan instance.");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkInstance);
    uint32_t cnt = MAX_GPUS;
    vk::PhysicalDevice pds[MAX_GPUS];
    VK_ASSERT(vkInstance.enumeratePhysicalDevices(&cnt, pds), "Failed to enumerate gpus");
    gpus = std::make_unique<gpu[]>(cnt);
    gpuCount = 0;
    const char* skipGpusRaw = configuration::getOption("nogpuid");//csv list of indexes
    bool allowNonDiscrete = configuration::contains("allowNonDiscreteGpu");
    char skipGpus[128];
    if(skipGpusRaw) {
      ASSERT_TRAP(std::strlen(skipGpusRaw) < 128, "skip gpu list too long");
      WITE::strcpy(skipGpus, skipGpusRaw, 128);
    } else {
      skipGpus[0] = 0;
    }
    char* remaining;
    for(long i = 0;i < cnt;i++) {
      bool skip = false;
      if(skipGpus[0]) {
	remaining = skipGpus;
	while(!skip && *remaining) {
	  long skipId = std::strtol(remaining, &remaining, 10);
	  skip = i == skipId;
	  if(*remaining) remaining++;//skip delim
	}
	//TODO skip gpus that don't support one of the requested extensions
      }
      if(!skip && !allowNonDiscrete) {
	vk::PhysicalDeviceProperties2 pvp;
	pds[i].getProperties2(&pvp);
	if(pvp.properties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu)
	  skip = true;
      }
      if(!skip) {
	new(&gpus[gpuCount])gpu(gpuCount, pds[i], deviceExtensions.size(), deviceExtensions.data());
	gpuCount++;
      } else {
	WARN("skipped gpu ", i, " because it's on the skip list: ", skipGpus);
      }
    }
    if(gpuCount < 1) {
      CRASH("No gpu found.");
    } else if(gpuCount == 1) {
      //loader code abstracts the dispatcher per-device. We can ask it to skip that if we only have one device.
      //MAYBE eventually have a dispatcher copy for each device but it'd have to be injected into EVERY vulkan api call using a device
      VULKAN_HPP_DEFAULT_DISPATCHER.init(gpus[gpuCount-1].getVkDevice());
    }
  };

  gpu& gpu::get(size_t i) {//static
    if(i > gpuCount) [[unlikely]] i = i % gpuCount;
    return gpus[i];
  }

}
