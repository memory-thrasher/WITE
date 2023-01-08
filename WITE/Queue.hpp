#pragma once

#include <memory>
#include <set>

#include "Thread.hpp"
#include "RecyclingPool.hpp"
#include "SyncLock.hpp"
#include "types.hpp"
#include "Vulkan.hpp"
#include "Semaphore.hpp"
#include "VulkanConverters.hpp"

namespace WITE::GPU {

  class Gpu;
  class ElasticCommandBuffer;

  class Queue {
  public:
    typedef struct Cmd_t {
      vk::CommandBuffer cmd;
      std::set<Semaphore*> waits, signals;
      //semaphores are defined for resources that need synchronization
    } Cmd;
  private:
    class Gpu* gpu;
    size_t queueInstanceCount;
    std::unique_ptr<vk::Queue[]> queueInstances;
    std::unique_ptr<Util::SyncLock[]> queueMutexes;
    struct PerThreadResources {
      vk::CommandPool pool;
      uint32_t idx;
      Collections::RecyclingPool<Cmd, false> cmdPool;
    };
    Platform::ThreadResource<PerThreadResources> pools;
    PerThreadResources* makePerThreadResources();
    void freePerThreadResources(PerThreadResources*);
    [[nodiscard]] Cmd* getNext();
    friend class ElasticCommandBuffer;
    void submit(Cmd* b, uint64_t frame);
  public:
    const uint32_t family;
    const vk::QueueFamilyProperties qfp;
    Queue(Gpu*, const struct vk::DeviceQueueCreateInfo&, vk::QueueFamilyProperties& qfp);
    ~Queue();
    //no destructor bc the global destroy will handle that
    ElasticCommandBuffer createBatch();
    ElasticCommandBuffer createBatch(uint64_t frame);
    inline Gpu* getGpu() { return gpu; };
    inline bool supportsTransfer() { return nonzero(qfp.queueFlags & vk::QueueFlagBits::eTransfer); };
    inline bool supportsCompute() { return nonzero(qfp.queueFlags & vk::QueueFlagBits::eCompute); };
    inline bool supportsGraphics() { return nonzero(qfp.queueFlags & vk::QueueFlagBits::eGraphics); };
    inline bool supports(QueueType qt) { return nonzero(qfp.queueFlags & queueBitByType[qt]); };
  };

}
