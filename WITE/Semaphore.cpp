#include "Semaphore.hpp"
#include "FrameCounter.hpp"
#include "PerGpu.hpp"
#include "Math.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  std::atomic_uint64_t Semaphore::idSeed;//static
  PerGpu<Semaphore>::creator_t Semaphore::factory = PerGpu<Semaphore>::creator_t_F::make(Semaphore::make);

  Semaphore::Semaphore(Gpu* gpu) : id(idSeed++), dev(gpu) {
    vk::SemaphoreTypeCreateInfo tci {
      vk::SemaphoreType::eTimeline,
      targetValue = Util::max(Util::FrameCounter::getFrame(), 1)-1
    };
    vk::SemaphoreCreateInfo ci { {}, reinterpret_cast<const void*>(&tci) };
    VK_ASSERT(gpu->getVkDevice().createSemaphore(&ci, ALLOCCB, &sem), "failed to create semaphore");
  };

  Semaphore::Semaphore(Semaphore&& o) : targetValue(o.targetValue), id(o.id) {
    o.sem = VK_NULL_HANDLE;
  };

  Semaphore::~Semaphore() {
    if(sem)
      dev->getVkDevice().destroySemaphore(sem);
    sem = VK_NULL_HANDLE;
  };

  uint64_t Semaphore::notePromise() {
    notePromise(Util::FrameCounter::getFrame());
    return targetValue;
  };

  void Semaphore::notePromise(uint64_t v) {
    if(v > targetValue)
      targetValue = v;
  };

  void Semaphore::signalNow() {
    notePromise();
    vk::SemaphoreSignalInfo si { sem, targetValue };
    VK_ASSERT(dev->getVkDevice().signalSemaphore(&si), "Failed to signal semaphore");
  };

  void Semaphore::signalNow(uint64_t v) {
    notePromise(v);
    vk::SemaphoreSignalInfo si { sem, v };
    VK_ASSERT(dev->getVkDevice().signalSemaphore(&si), "Failed to signal semaphore");
  };

  uint64_t Semaphore::getCurrentValue() {
    uint64_t ret;
    VK_ASSERT(dev->getVkDevice().getSemaphoreCounterValue(sem, &ret), "Failedto query semaphore");
    return ret;
  };

  void Semaphore::make(Semaphore* ret, size_t gpuIdx) {//static
    new(ret)Semaphore(&Gpu::get(gpuIdx));
  };

}
