#pragma once

namespace WITE {
  class Queue {
  public:
    class ExecutionPlan {
    public:
      virtual void beginParallel() = 0;
      virtual void beginSerial() = 0;
      virtual void beginReduce() = 0;
      virtual GPU* getGpu() = 0;
      virtual void beginRenderPass(RenderPass*) = 0;
      virtual void endRenderPass() = 0;
      //TODO create*pipeline, renderpass, bind {pipeline, descriptor set, vertex buffer (?or use global?)}, draw
    protected:
      ExecutionPlan(ExecutionPlan&) = delete;
      ExecutionPlan() = default;
    };
    static Queue* get(size_t idx);
    virtual ExecutionPlan* getComplexPlan() = 0;//EP is a thread-specific resource. Do not store or transfer between threads.
  protected:
    Queue(Queue&) = delete;
    Queue() = default;
  };
}
