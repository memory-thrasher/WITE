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

  class Queue {
  private:
    class Gpu* gpu;
    size_t queueInstanceCount;
    std::unique_ptr<vk::Queue[]> queueInstances;
    std::unique_ptr<Util::SyncLock[]> queueMutexes;
    struct PerThreadResources {
      vk::CommandPool pool;
      uint32_t idx;
      Collections::RecyclingPool<vk::CommandBuffer, false> cmdPool;
    };
    Platform::ThreadResource<PerThreadResources> pools;
    PerThreadResources* makePerThreadResources();
    void freePerThreadResources(PerThreadResources*);
  public:
    [[nodiscard]] cmd_t getNext();
    void free(cmd_t);
    void submit(const vk::SubmitInfo2&);
    const uint32_t family;
    const vk::QueueFamilyProperties qfp;
    Queue(Gpu*, const struct vk::DeviceQueueCreateInfo&, vk::QueueFamilyProperties& qfp);
    ~Queue();
    //no destructor bc the global destroy will handle that
    void present(const vk::PresentInfoKHR* pi);
    inline Gpu* getGpu() { return gpu; };
    size_t getGpuIdx();
    inline bool supportsTransfer() { return nonzero(qfp.queueFlags & vk::QueueFlagBits::eTransfer); };
    inline bool supportsCompute() { return nonzero(qfp.queueFlags & vk::QueueFlagBits::eCompute); };
    inline bool supportsGraphics() { return nonzero(qfp.queueFlags & vk::QueueFlagBits::eGraphics); };
    inline bool supports(QueueType qt) { return nonzero(qfp.queueFlags & queueBitByType[qt]); };
  };

}
