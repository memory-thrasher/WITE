#pragma once

#include <memory>

#include "Thread.hpp"
#include "DynamicRollingQueue.hpp"
#include "SyncLock.hpp"
#include "types.hpp"
#include "Vulkan.hpp"

namespace WITE::GPU {

  class Gpu;
  class ElasticCommandBuffer;

  class Queue {
  private:
    class Gpu* gpu;
    size_t queueInstanceCount;
    std::unique_ptr<vk::Queue[]> queueInstances;
    std::unique_ptr<Util::SyncLock[]> queueMutexes;
    struct PerThreadResources {
      vk::CommandPool pool;
      uint32_t idx;
      struct Cmd {
	vk::CommandBuffer cmd;
	logicalDeviceMask_t ldm;
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
    void submit(ElasticCommandBuffer* b);
  public:
    const uint32_t family;
    Queue(Gpu*, const struct vk::DeviceQueueCreateInfo&);
    ElasticCommandBuffer createBatch(ElasticCommandBuffer* previousLink = NULL);
    Gpu* getGpu() { return gpu; };
  };

}
