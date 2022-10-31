#pragma once

#include <memory>

#include "Thread.hpp"
#include "DynamicRollingQueue.hpp"
#include "SyncLock.hpp"

#include "Vulkan.hpp"

namespace WITE::GPU {

  class Gpu;
  class ElasticCommandBuffer;

  enum class QueueType { eGraphics, eTransfer, eCompute };

  class Queue {
  private:
    class Gpu* gpu;
    size_t queueInstanceCount;
    uint32_t family;
    std::unique_ptr<vk::Queue[]> queueInstances;
    std::unique_ptr<Util::SyncLock[]> queueMutexes;
    struct PerThreadResources {
      vk::CommandPool pool;
      uint32_t idx;
      struct Cmd {
	vk::CommandBuffer cmd;
	uint64_t ldm;
	//TODO more stuff like fences and semaphores
	//TODO default constructor
      };
      Collections::DynamicRollingQueue<Cmd> cachedCmds;
    };
    Platform::ThreadResource<PerThreadResources> pools;//TODO allocator/deallocator
    PerThreadResources* makePerThreadResources();
    void freePerThreadResources(PerThreadResources*);
    [[nodiscard]] vk::CommandBuffer getNext();
    friend class ElasticCommandBuffer;
  public:
    Queue(Gpu*, const struct vk::DeviceQueueCreateInfo&);
    ElasticCommandBuffer createBatch(uint64_t ldm);
    Gpu* getGpu() { return gpu; };
  };

}
