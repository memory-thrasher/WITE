#pragma once

#include <vector>
#include <set>

#include "Vulkan.hpp"
#include "Queue.hpp"

namespace WITE::GPU {

  class Gpu;
  class Semaphore;

  class ElasticCommandBuffer {
  private:
    Queue::Cmd* cmd;
    Queue* owner;
    uint64_t frame;//for truck method to signal end of frame availability on a resource that was not actually ready before the next frame started
    std::array<ElasticCommandBuffer*, 2> children;
    friend class Queue;
    inline bool supports(QueueType qt) { return owner && owner->supports(qt); };
  public:
    ~ElasticCommandBuffer();
    ElasticCommandBuffer(Queue* owner, uint64_t frame);
    ElasticCommandBuffer(const ElasticCommandBuffer&) = delete;
    ElasticCommandBuffer(ElasticCommandBuffer&) = delete;
    ElasticCommandBuffer() : owner(NULL) {};
    inline operator vk::CommandBuffer() { return cmd->cmd; };
    inline vk::CommandBuffer* operator->() { return &cmd->cmd; };
    inline Gpu* getGpu() { return owner->getGpu(); };
    inline Queue* getQueue() { return owner; };

    //but only on the subresource corrosponding to this->getGpu
    void addDependency(Semaphore*);
    void addSignal(Semaphore*);

    //these 4 are for dependent operations.
    //They may alias *this, or they may be dynamically created on another queue of the same device
    //In the case of children cmds, this buffer will be queued before any children. getter calls can be layered.
    //example: to do a grahics operation, strongly-before a transfer operation, strongly-before another graphics operation, do
    //cmd.getGraphics().A(); cmd.getGraphics().getTransfer().B(); cmd.GetGraphics().getTransfer().getGraphics.C();
    //(better yet cache the intermediate references) but if C does not actually depend on B, then do C and A on the primary cmd
    //this way, if the graphics queue also supports transfer operations, all 3 ops get recorded to the same cmd buffer in order
    //if not, it turns into 3 commandbuffers with semaphores keeping them in order
    //DO NOT split a render pass into pieces like this, special handling still required, see RenderTarget
    inline ElasticCommandBuffer& getGraphics() { return getOfType(QueueType::eGraphics); };
    inline ElasticCommandBuffer& getTransfer() { return getOfType(QueueType::eTransfer); };
    inline ElasticCommandBuffer& getCompute() { return getOfType(QueueType::eCompute); };
    ElasticCommandBuffer& getOfType(QueueType);
  };

}
