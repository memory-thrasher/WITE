#pragma once

class Queue : public WITE::Queue {
public:
  static constexpr VkSemaphoreCreateInfo SEMAPHORE_CREATE_INFO = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, 0, 0 };
  static constexpr VkCommandBufferBeginInfo CMDBUFFER_BEGIN_INFO = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, VK_NULL_HANDLE, 0, VK_NULL_HANDLE};
  class ExecutionPlan : public WITE::Queue::ExecutionPlan {//thread-specific resource: do *not* pass between threads. Locks queue once any begin is called, until isRunning has returned false
  public:
    ExecutionPlan(Queue* master);
    ~ExecutionPlan();
    void beginParallel();//returns an empty commandbuffer in the recording state with no pre-dependencies
    void beginSerial();//returns an empty commandbuffer in the recording state predependent on only the most recently returned commandbuffer
    void beginReduce();//returns an empty commandbuffer in the recording state predependent on all previously returned commandbuffers.
    VkCommandBuffer getActive();
    void popStateSemaphores(std::vector<VkSemaphore>*);
    void submit();//submit the plan for execution
    bool isRunning();
    void queueWaitForSemaphore(VkSemaphore sem);
  private:
    constexpr static size_t MAX_QUEUE_REDUCE = 16;//use case atm, this is the max number of cameras per window
    typedef struct {
      VkCommandBuffer cmd;
      VkSemaphore completionSemaphore;
      VkSemaphore waitedSemaphores[MAX_QUEUE_REDUCE];
      VkPipelineStageFlags waitedSem_PipeStages[MAX_QUEUE_REDUCE];
    } SerialQueue;
    std::vector<SerialQueue*> allocated;
    size_t head = 0, allocIdx = 0;
    std::vector<VkSemaphore> headSems;
    std::vector<VkSubmitInfo> allSubmits;
    Queue* queue;
    VkFence fence;
    bool activeRender = false;
    size_t beginInternal();
  };
  Queue(GPU* gpu, uint32_t family, uint32_t idx);
  ~Queue();
  ExecutionPlan* getComplexPlan();//EP is a thread-specific resource
  void submitEP(); //submit the EP for this thread if one exists, otherwise noop
  VkCommandBuffer makeCmd();
  VkQueue getQueue() { return queue; };
  void destroyCmd(VkCommandBuffer);
  void submit(VkCommandBuffer* cmds, size_t cmdLen, VkFence fence = VK_NULL_HANDLE);
  inline void submit(VkCommandBuffer cmd, VkFence fence = VK_NULL_HANDLE) { submit(&cmd, 1, fence); };
  bool supports(VkSurfaceKHR*);
  static void submitAllForThisThread();
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
  ExecutionPlan* makeComplexPlan();
  size_t cmdCount = 0;
  friend class ExecutionPlan;
};


