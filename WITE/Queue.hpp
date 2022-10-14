#include <memory>

#include "Thread.hpp"
#include "DynamicRollingQueue.hpp"

#include "Vulkan.hpp"

namespace WITE::GPU {

  class Queue {
  private:
    size_t queueInstanceCount;
    std::unique_ptr<vk::Queue[]> queueInstances;
    std::unique_ptr<Util::SyncLock[]> queueMutexes;//TODO make this one array of struct with both queue and mutex
    struct PerThreadResources {
      vk::CommandPool pool;
      struct Cmd {
	vk::CommandBuffer cmd;
	//TODO more stuff like fences and semaphores
	//TODO default constructor
      };
      Collections::DynamicRollingQueue<Cmd> cachedCmds;
    };
    Platform::ThreadResource<PerThreadResources> pools;//TODO allocator/deallocator
  public:
    vk::CommandBuffer getAvailable();
  };

}
