#include <vector>
#include <map>
#include <bit>

#include "Gpu.hpp"
#include "Queue.hpp"
#include "BitmaskIterator.hpp"
#include "Math.hpp"
#include "RenderTarget.hpp"
#include "DEBUG.hpp"

#include "Database.hpp" //for DB_THREAD_COUNT

const std::map<vk::MemoryPropertyFlags, int64_t> memoryScoreByFlag =
  {
    { vk::MemoryPropertyFlagBits::eDeviceLocal      ,   256 },
    { vk::MemoryPropertyFlagBits::eHostVisible      ,   -64 },
    { vk::MemoryPropertyFlagBits::eHostCoherent     ,     8 },
    { vk::MemoryPropertyFlagBits::eHostCached       ,    16 },
    { vk::MemoryPropertyFlagBits::eLazilyAllocated  ,  -128 },
    { vk::MemoryPropertyFlagBits::eProtected        ,     0 },
    { vk::MemoryPropertyFlagBits::eDeviceCoherentAMD,   256 },
    { vk::MemoryPropertyFlagBits::eDeviceUncachedAMD,   -32 }
  };

const std::map<const WITE::GPU::ImageFormatFamily, const std::vector<vk::Format>> formatsByFamily =
  {
   { { 2, 4 }, { vk::Format::eR4G4UnormPack8 } },
   { { 4, 4 }, { vk::Format::eR4G4B4A4UnormPack16 } },
   { { 1, 8 }, { vk::Format::eR8Unorm, vk::Format::eR8Snorm,
		 vk::Format::eR8Uscaled, vk::Format::eR8Sscaled,
		 vk::Format::eR8Uint, vk::Format::eR8Sint } },
   { { 2, 8 }, { vk::Format::eR8G8Unorm, vk::Format::eR8G8Snorm,
		 vk::Format::eR8G8Uscaled, vk::Format::eR8G8Sscaled,
		 vk::Format::eR8G8Uint, vk::Format::eR8G8Sint } },
   { { 3, 8 }, { vk::Format::eR8G8B8Unorm, vk::Format::eR8G8B8Snorm,
		 vk::Format::eR8G8B8Uscaled, vk::Format::eR8G8B8Sscaled,
		 vk::Format::eR8G8B8Uint, vk::Format::eR8G8B8Sint,
		 vk::Format::eB8G8R8Unorm, vk::Format::eB8G8R8Snorm,
		 vk::Format::eB8G8R8Uscaled, vk::Format::eB8G8R8Sscaled,
		 vk::Format::eB8G8R8Uint, vk::Format::eB8G8R8Sint } },
   { { 4, 8 }, { vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Snorm,
		 vk::Format::eR8G8B8A8Uscaled, vk::Format::eR8G8B8A8Sscaled,
		 vk::Format::eR8G8B8A8Uint, vk::Format::eR8G8B8A8Sint,
		 vk::Format::eB8G8R8A8Unorm, vk::Format::eB8G8R8A8Snorm,
		 vk::Format::eB8G8R8A8Uscaled, vk::Format::eB8G8R8A8Sscaled,
		 vk::Format::eB8G8R8A8Uint, vk::Format::eB8G8R8A8Sint } },
   // { { 3, 10}, { vk::Format::eA2R10G10B10UnormPack32, vk::Format::eA2R10G10B10SnormPack32,
   // 		 vk::Format::eA2R10G10B10UscaledPack32, vk::Format::eA2R10G10B10SscaledPack32,
   // 		 vk::Format::eA2R10G10B10UintPack32, vk::Format::eA2R10G10B10SintPack32,
   // 		 vk::Format::eA2B10G10R10UnormPack32, vk::Format::eA2B10G10R10SnormPack32,
   // 		 vk::Format::eA2B10G10R10UscaledPack32, vk::Format::eA2B10G10R10SscaledPack32,
   // 		 vk::Format::eA2B10G10R10UintPack32, vk::Format::eA2B10G10R10SintPack32 } },
   { { 1, 16}, { vk::Format::eR16Unorm, vk::Format::eR16Snorm,
		 vk::Format::eR16Uscaled, vk::Format::eR16Sscaled,
		 vk::Format::eR16Uint, vk::Format::eR16Sint,
		 vk::Format::eR16Sfloat } },
   { { 2, 16}, { vk::Format::eR16G16Unorm, vk::Format::eR16G16Snorm,
		 vk::Format::eR16G16Uscaled, vk::Format::eR16G16Sscaled,
		 vk::Format::eR16G16Uint, vk::Format::eR16G16Sint,
		 vk::Format::eR16G16Sfloat } },
   { { 3, 16}, { vk::Format::eR16G16B16Unorm, vk::Format::eR16G16B16Snorm,
		 vk::Format::eR16G16B16Uscaled, vk::Format::eR16G16B16Sscaled,
		 vk::Format::eR16G16B16Uint, vk::Format::eR16G16B16Sint,
		 vk::Format::eR16G16B16Sfloat } },
   { { 4, 16}, { vk::Format::eR16G16B16A16Unorm, vk::Format::eR16G16B16A16Snorm,
		 vk::Format::eR16G16B16A16Uscaled, vk::Format::eR16G16B16A16Sscaled,
		 vk::Format::eR16G16B16A16Uint, vk::Format::eR16G16B16A16Sint,
		 vk::Format::eR16G16B16A16Sfloat } },
   { { 1, 32}, { vk::Format::eR32Uint, vk::Format::eR32Sint,
		 vk::Format::eR32Sfloat } },
   { { 2, 32}, { vk::Format::eR32G32Uint, vk::Format::eR32G32Sint,
		 vk::Format::eR32G32Sfloat } },
   { { 3, 32}, { vk::Format::eR32G32B32Uint, vk::Format::eR32G32B32Sint,
		 vk::Format::eR32G32B32Sfloat } },
   { { 4, 32}, { vk::Format::eR32G32B32A32Uint, vk::Format::eR32G32B32A32Sint,
		 vk::Format::eR32G32B32A32Sfloat } },
   { { 1, 64}, { vk::Format::eR64Uint, vk::Format::eR64Sint,
		 vk::Format::eR64Sfloat } },
   { { 2, 64}, { vk::Format::eR64G64Uint, vk::Format::eR64G64Sint,
		 vk::Format::eR64G64Sfloat } },
   { { 3, 64}, { vk::Format::eR64G64B64Uint, vk::Format::eR64G64B64Sint,
		 vk::Format::eR64G64B64Sfloat } },
   { { 4, 64}, { vk::Format::eR64G64B64A64Uint, vk::Format::eR64G64B64A64Sint,
		 vk::Format::eR64G64B64A64Sfloat } },
  };

namespace WITE::GPU {

  //statics
  std::atomic_bool Gpu::inited = false;
  vk::Instance Gpu::vkInstance;
  size_t Gpu::gpuCount;
  std::unique_ptr<Gpu[]> Gpu::gpus;
  deviceMask_t Gpu::logicalPhysicalDeviceMatrix[MAX_LDMS];
  std::unique_ptr<struct Gpu::LogicalGpu[]> Gpu::logicalDevices;
  size_t Gpu::logicalDeviceCount;
  bool Gpu::running;

  Gpu::Gpu(size_t idx, vk::PhysicalDevice pv) : idx(idx), pv(pv) {
    pv.getProperties2(&pvp);
    uint32_t cnt = 0;
    pv.getQueueFamilyProperties2(&cnt, NULL);
    std::unique_ptr<vk::QueueFamilyProperties2[]> temp_qfp = std::make_unique<vk::QueueFamilyProperties2[]>(cnt);
    qfp = std::make_unique<qfp_t[]>(cnt);
    for(size_t i = 0;i < cnt;i++) {
      temp_qfp[i].pNext = reinterpret_cast<void*>(&qfp[i].p);
      qfp[i].p.pNext = reinterpret_cast<void*>(&qfp[i].gpp);
    }
    pv.getQueueFamilyProperties2(&cnt, temp_qfp.get());
    bool hasGraphics = false, hasTransfer = false, hasCompute = false;
    uint32_t graphics, transfer, compute;
    for(size_t i = 0;i < cnt;i++) {
      bool g = (uint32_t)(qfp[i].p.queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics);
      bool t = (uint32_t)(qfp[i].p.queueFamilyProperties.queueFlags & vk::QueueFlagBits::eTransfer);
      bool c = (uint32_t)(qfp[i].p.queueFamilyProperties.queueFlags & vk::QueueFlagBits::eCompute);
      hasGraphics |= g;
      hasTransfer |= t;
      hasCompute |= c;
      if(g && c) {
	graphics = compute = i;//prefer g+c so a single render pass can have both options
	if(t) {
	  transfer = i;
	  break;//all 3 = optimal
	}
      } else if(g && t) {
	if(!hasGraphics || !hasCompute || compute != graphics)
	  graphics = transfer = i;
        else if(!hasTransfer)
	  transfer = i;
      } else {
	if(g && !hasGraphics)
	  graphics = i;
	if(c && !hasCompute)
	  compute = i;
	if(t && !hasTransfer)
	  transfer = i;
      }
    }
    if(!hasGraphics || !hasTransfer || !hasCompute)
      CRASH_PREINIT("Cannot find one queue of each type on gpu");
    if(graphics != compute)
      WARN("WARN: gpu does not have a queue that supports both graphics and compute capability, render passes that include both cannot happen.");//ATM this case will cause a crash if/when such an RP happens. Even once that is implemented, this case has potential log value.
    cnt = 3;
    if(graphics == transfer || graphics == compute) cnt--;
    if(transfer == compute) cnt--;
    vk::DeviceQueueCreateInfo dqcis[3];
    float priorities[DB_THREAD_COUNT];
    for(size_t i = 0;i < DB_THREAD_COUNT;i++) priorities[i] = 0.5;
    size_t i = 0;
    size_t go, co, to = go = co = 0;
    dqcis[i] = vk::DeviceQueueCreateInfo((vk::DeviceQueueCreateFlags)0, graphics,
					 Util::min(qfp[graphics].p.queueFamilyProperties.queueCount, DB_THREAD_COUNT),
					 priorities);
    i++;
    if(graphics != compute) {
      dqcis[i] = vk::DeviceQueueCreateInfo((vk::DeviceQueueCreateFlags)0, compute,
					   Util::min(qfp[compute].p.queueFamilyProperties.queueCount, DB_THREAD_COUNT),
					   priorities);
      co = i++;
      if(compute == transfer) to = co;
    }
    if(transfer != graphics && transfer != compute) {
      dqcis[i] = vk::DeviceQueueCreateInfo((vk::DeviceQueueCreateFlags)0, transfer,
					   Util::min(qfp[transfer].p.queueFamilyProperties.queueCount, DB_THREAD_COUNT),
					   priorities);
      to = i++;
    }

    vk::PhysicalDeviceFeatures pdf;
    pdf.geometryShader = true;
    pdf.tessellationShader = true;
    pdf.logicOp = true;
    pdf.depthBounds = true;
    pdf.vertexPipelineStoresAndAtomics = true;
    pdf.fragmentStoresAndAtomics = true;
    pdf.shaderStorageImageMultisample = true;
    pdf.shaderClipDistance = true;
    pdf.shaderCullDistance = true;
    pdf.shaderFloat64 = true;
    pdf.shaderInt64 = true;
    pdf.shaderInt16 = true;
    vk::PhysicalDeviceVulkan11Features pdf11;
    pdf11.storageBuffer16BitAccess = true;
    pdf11.uniformAndStorageBuffer16BitAccess = true;
    pdf11.storagePushConstant16 = true;
    pdf11.storageInputOutput16 = true;
    pdf11.multiview = true;//TODO read more on this
    pdf11.multiviewGeometryShader = true;
    pdf11.multiviewTessellationShader = true;
    vk::PhysicalDeviceVulkan12Features pdf12;
    pdf11.pNext = reinterpret_cast<void*>(&pdf12);
    pdf12.storageBuffer8BitAccess = true;
    pdf12.uniformAndStorageBuffer8BitAccess = true;
    pdf12.storagePushConstant8 = true;
    pdf12.shaderBufferInt64Atomics = true;
    pdf12.shaderSharedInt64Atomics = true;
    pdf12.shaderInt8 = true;
    vk::DeviceCreateInfo dci((vk::DeviceCreateFlags)0, cnt, dqcis);
    dci.pEnabledFeatures = &pdf;
    dci.pNext = reinterpret_cast<void*>(&pdf11);
    VK_ASSERT(pv.createDevice(&dci, ALLOCCB, &dev), "Failed to create device");

    queues = std::unique_ptr<Queue[]>(calloc<Queue>(cnt));
    for(size_t i = 0;i < cnt;i++)
      new(&queues[i])Queue(this, dqcis[i], qfp[dqcis[i].queueFamilyIndex].p.queueFamilyProperties);
    this->graphics = &queues[go];
    this->transfer = &queues[to];
    this->compute = &queues[co];

    vk::FormatProperties fp;
    for(auto pair : formatsByFamily)
      for(auto format : pair.second) {
	pv.getFormatProperties(format, &fp);
	formatProperties[format] = fp;//explicit assignment operator
      }

    //TODO look into save/load the cache (create info takes pointer to loaded data))
    vk::PipelineCacheCreateInfo pipecacheci((vk::PipelineCacheCreateFlags)0, 0, NULL);
    VK_ASSERT(dev.createPipelineCache(&pipecacheci, ALLOCCB, &pipelineCache), "failed to create pipeline cache");

    pdmp.pNext = &pdmbp;
    pv.getMemoryProperties2(&pdmp);
    for(size_t i = 0;i < pdmp.memoryProperties.memoryTypeCount;i++) {
      int64_t score = 0;
      Collections::BitmaskIterator j(vk::MemoryPropertyFlags::MaskType(pdmp.memoryProperties.memoryTypes[i].propertyFlags));
      while(j) {
	auto flag = j.asMask<vk::MemoryPropertyFlags>();
	if(memoryScoreByFlag.contains(flag))
	  score += memoryScoreByFlag.at(flag);
	++j;
      };
      memoryScoreByType[i] = score;
    }
    for(size_t i = 0;i < pdmp.memoryProperties.memoryHeapCount;i++)
      freeMemoryByHeap[i] = pdmbp.heapBudget[i] - pdmbp.heapUsage[i];
  };

  void Gpu::allocate(const vk::MemoryRequirements& mr, VRam* out) {
    uint8_t type;
    int64_t bestScore = std::numeric_limits<int64_t>::lowest();
    for(auto t : Collections::BitmaskIterator(mr.memoryTypeBits)) {
      if(memoryScoreByType[t] > bestScore && freeMemoryByHeap[pdmp.memoryProperties.memoryTypes[t].heapIndex] > mr.size) {
	bestScore = memoryScoreByType[t];
	type = t;
      }
    }
    new(out)VRam(mr, type, this);
    freeMemoryByHeap[pdmp.memoryProperties.memoryTypes[type].heapIndex] -= mr.size;
  };

  void Gpu::deallocate(VRam* doom) {
    doom->~VRam();
    freeMemoryByHeap[pdmp.memoryProperties.memoryTypes[doom->mai.memoryTypeIndex].heapIndex] += doom->mai.allocationSize;
  };

  void Gpu::shutdown() {
    running = false;
  };

  void Gpu::init(size_t logicalDeviceCount, const float* priorities) {//static
    if(inited.exchange(true))
      return;
    running = true;
    vk::ApplicationInfo appI("WITE Engine", 0, "WITE Engine", 0, (1 << 22) | (3 << 12) | (216));//TODO accept app name
    vk::InstanceCreateInfo ci((vk::InstanceCreateFlags)0, &appI);
#ifdef DEBUG
    assert(ci.enabledLayerCount == 0);
    ci.enabledLayerCount = 1;
    const char* ln = "VK_LAYER_KHRONOS_validation";
    ci.ppEnabledLayerNames = &ln;
    //TODO other extensions. Doesn't present require an extension?
#endif
    VK_ASSERT(vk::createInstance(&ci, ALLOCCB, &vkInstance), "Failed to create vulkan instance.");
    uint32_t cnt = MAX_GPUS;
    vk::PhysicalDevice pds[MAX_GPUS];
    VK_ASSERT(vkInstance.enumeratePhysicalDevices(&cnt, pds), "Failed to enumerate gpus");
    gpus = std::unique_ptr<Gpu[]>(calloc<Gpu>(cnt));
    for(size_t i = 0;i < cnt;i++)
      new(&gpus[i])Gpu(i, pds[i]);
    gpuCount = cnt;
    memset(logicalPhysicalDeviceMatrix, 0, sizeof(logicalPhysicalDeviceMatrix));
    if(gpuCount < 1) {
      CRASH("No gpu found.");
    } else if(gpuCount == 1) {
      for(size_t i = 0;i < logicalDeviceCount;i++)
	logicalPhysicalDeviceMatrix[i] = 1;
    } else {
      //TODO find gpu performance ability and vary capacity for priority
      float pPerRealGpu = Util::mangle<Util::Mangle_Sum<float>, float>(priorities, logicalDeviceCount) / gpuCount,
	rollingSum = 0;
      for(size_t i = 0;i < logicalDeviceCount;i++) {
	size_t min = Util::floor<size_t>(rollingSum/pPerRealGpu + std::numeric_limits<float>::lowest());
	rollingSum += priorities[i];
	size_t max = Util::floor<size_t>(rollingSum/pPerRealGpu - std::numeric_limits<float>::lowest());
	ASSERT_TRAP(min <= max, "Bah");
	for(size_t j = min;j <= max;j++)
	  logicalPhysicalDeviceMatrix[i] |= 1 << j;
      }
    }
    logicalDevices = std::make_unique<struct LogicalGpu[]>(logicalDeviceCount);
    Gpu::logicalDeviceCount = logicalDeviceCount;
    struct LogicalGpu blank;
    for(size_t i = 0;i < logicalDeviceCount;i++) {
      struct LogicalGpu& l = logicalDevices[i];
      Collections::IteratorWrapper<Collections::BitmaskIterator>
	gpuIt((Collections::BitmaskIterator(logicalPhysicalDeviceMatrix[i])), (Collections::BitmaskIterator()));
      for(auto kvp : gpus[*gpuIt].formatProperties) {
	auto fp = kvp.second;
	gpuIt++;
	for(size_t gpuNext : gpuIt) {
	  auto limiter = gpus[gpuNext].formatProperties[kvp.first];
	  fp.linearTilingFeatures &= limiter.linearTilingFeatures;
	  fp.optimalTilingFeatures &= limiter.optimalTilingFeatures;
	  fp.bufferFeatures &= limiter.bufferFeatures;
	}
	l.formatProperties[kvp.first] = fp;
      }
    }
    RenderTarget::SpawnTruckThread();
  };

  Gpu& Gpu::get(size_t i) {//static
    return gpus[i];
  }

  uint8_t Gpu::gpuCountByLdm(logicalDeviceMask_t ldm) {//static
    return std::popcount(gpuMaskByLdm(ldm));
  }

  Platform::ThreadResource<uint64_t> ldmSalt;

  Gpu* Gpu::getGpuFor(logicalDeviceMask_t ldm) {//static
    deviceMask_t options = gpuMaskByLdm(ldm);
    auto pc = std::popcount(options);
    if(pc <= 0) return NULL;
    if(pc == 1) return &get(Util::unroll(options));
    return &get(Util::unrollNth(options, (*ldmSalt.get())++ % pc));
  };

  deviceMask_t Gpu::gpuMaskByLdm(logicalDeviceMask_t ldm) {//static
    deviceMask_t ret = 0;
    for(size_t i = 0;i < MAX_LDMS;i++)
      if(ldm & (1 << i))
	ret |= logicalPhysicalDeviceMatrix[i];
    return ret;
  }

  vk::Format Gpu::getBestImageFormat(uint8_t comp, uint8_t compSize, usage_t usage, logicalDeviceMask_t ldm) {
    //TODO better definition of "best", for now just use first that checks all the boxes
    vk::FormatFeatureFlags features = usageFeatures(usage);
    for(vk::Format format : formatsByFamily.at({comp, compSize})) {
      bool supported = true;
      for(size_t i : Collections::IteratorWrapper(Collections::BitmaskIterator(ldm), {}))
	if((logicalDevices[i].formatProperties[format].optimalTilingFeatures & features) != features) {
	  supported = false;
	  break;
	}
      if(supported)
	return format;
    }
    WARN("Suitable format not found");
    return vk::Format::eUndefined;
  };

  Queue* Gpu::getQueue(QueueType qt) {
    switch(qt) {
    case QueueType::eCompute: return compute;
    case QueueType::eGraphics: return graphics;
    case QueueType::eTransfer: return transfer;
    }
  };

}
