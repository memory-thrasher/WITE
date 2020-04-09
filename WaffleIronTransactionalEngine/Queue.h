#pragma once

#include "stdafx.h"
#include "Thread.h"

class GPU;

class Queue {
public:
  static constexpr VkSemaphoreCreateInfo SEMAPHORE_CREATE_INFO = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, 0, 0 };
  static constexpr VkCommandBufferBeginInfo CMDBUFFER_BEGIN_INFO = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL};
  class ExecutionPlan {//thread-specific resource: do *not* pass between threads. Locks queue once any begin is called, until isRunning has returned false
  public:
    ExecutionPlan(Queue* master) : queue(master) {};
    ~ExecutionPlan();
    VkCommandBuffer beginParallel();//returns an empty commandbuffer in the recording state with no pre-dependencies
    VkCommandBuffer beginSerial();//returns an empty commandbuffer in the recording state predependent on only the most recently returned commandbuffer
    VkCommandBuffer beginReduce();//returns an empty commandbuffer in the recording state predependent on all previously returned commandbuffers.
    VkCommandBuffer getActive();
    void getStateSemaphores(VkSemaphore* outSems, size_t* outSemLen);
    void submit();//submit the plan for execution
    bool isRunning();
    void queueWaitForSemaphore(VkSemaphore sem);
  private:
    typedef struct {
      VkCommandBuffer cmd;
      //VkSubmitInfo submitInfo;//Moved to parallel vector for easier enquement
      VkSemaphore completionSemaphore;
      VkSemaphore* waitedSemaphores;//malloc, only used if this SerialQueue was created by beginReduce. Freed in EP destructor.
    } SerialQueue;
    std::vector<SerialQueue> allocated;
    size_t head, allocIdx = 0;
    std::vector<VkSemaphore> headSems;
    std::vector<VkSubmitInfo> allSubmits;
    Queue* queue;
    VkFence fence;
    bool activeRender;
    size_t beginInternal();
    std::unique_ptr<WITE::ScopeLock> hold;
  };
  Queue(GPU* gpu, uint32_t family, uint32_t idx);
  ~Queue();
  std::shared_ptr<ExecutionPlan> getComplexPlan();//EP is a thread-specific resource
  VkCommandBuffer makeCmd();
  VkQueue getQueue() { return queue; };
  void destroyCmd(VkCommandBuffer);
  void submit(VkCommandBuffer* cmds, size_t cmdLen, VkFence fence = NULL);
  inline void submit(VkCommandBuffer cmd, VkFence fence = NULL) { submit(&cmd, 1, fence); };
  bool supports(VkSurfaceKHR*);
  GPU* gpu;
  unsigned int family;
  VkQueue queue;//TODO private and getters for these
private:
  Queue(Queue& other) = delete;//no copy construct
  VkQueueFamilyProperties* properties;
  WITE::SyncLock lock;
  VkCommandPool cmdPool;//FIXME this could be moved to ExecutionPlan for better multi-threading (no more locks)
  VkCommandBufferAllocateInfo bufInfo;
  WITE::ThreadResource<ExecutionPlan> complexPlans;
  std::shared_ptr<ExecutionPlan> makeComplexPlan();
  friend class ExecutionPlan;
};


