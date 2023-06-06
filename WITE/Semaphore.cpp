#include "Semaphore.hpp"
#include "PerGpu.hpp"
#include "Math.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  std::atomic_uint64_t Semaphore::idSeed;//static
  PerGpu<Semaphore>::creator_t Semaphore::factory = PerGpu<Semaphore>::creator_t_F::make(Semaphore::make);

  Semaphore::Semaphore(Gpu* gpu) : id(idSeed++), dev(gpu) {
    vk::SemaphoreTypeCreateInfo tci {
      vk::SemaphoreType::eTimeline,
      targetValue = 0
    };
    vk::SemaphoreCreateInfo ci { {}, reinterpret_cast<const void*>(&tci) };
    VK_ASSERT(gpu->getVkDevice().createSemaphore(&ci, ALLOCCB, &sem), "failed to create semaphore");
    // LOG("create semaphore for gpu ", gpu->getIndex());
  };

  Semaphore::Semaphore(Semaphore&& o) : targetValue(o.targetValue.load()), id(o.id), sem(o.sem), dev(o.dev) {
    o.sem = VK_NULL_HANDLE;
  };

  void Semaphore::dispose() {
    if(sem) {
      // LOG("delete semaphore for gpu ", dev->getIndex());
      dev->getVkDevice().destroySemaphore(sem);
      sem = VK_NULL_HANDLE;
    }
  };

  Semaphore::~Semaphore() {
    dispose();
  };

  uint64_t Semaphore::notePromise() {
    return ++targetValue;
  };

  uint64_t Semaphore::getCurrentValue() {
    uint64_t ret;
    VK_ASSERT(dev->getVkDevice().getSemaphoreCounterValue(sem, &ret), "Failed to query semaphore");
    return ret;
  };

  void Semaphore::make(Semaphore* ret, size_t gpuIdx) {//static
    new(ret)Semaphore(&Gpu::get(gpuIdx));
  };

}
