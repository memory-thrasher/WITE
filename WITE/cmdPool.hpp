#pragma once

#include <queue>

#include "wite_vulkan.hpp"
#include "syncLock.hpp"

namespace WITE {

  //primary rendering and other complex work belongs in an onion, which manages it's own cmds. This is for one-offs and small async jobs. Not thread safe (except for in handling the shared queue); one pool per thread exists in the gpu class. Ensure no other thread attempts access. Do not wait the fence outside waitFor. Call waitFor at most once per submit. Do not resubmit.

  struct cmdPool;

  struct tempCmd {
    vk::CommandBuffer cmd;
    vk::Fence fence;
    cmdPool*pool;
    void submit();
    bool isPending();
    void waitFor();
    inline vk::CommandBuffer* operator->() { return &cmd; };
  };

  struct cmdPool {
    std::vector<tempCmd> all;
    std::queue<tempCmd> submitted;
    vk::CommandPool pool;
    vk::Device dev;
    vk::Queue q;
    syncLock* qMutex;

    cmdPool(vk::CommandPool pool, vk::Device dev, vk::Queue q, syncLock* qMutex);
    ~cmdPool();

    //for threadResource
    static cmdPool* forDevice(size_t idx);
    static cmdPool* lowPrioForDevice(size_t idx);

    tempCmd allocate();
    void waitFor();

  };

};
