#pragma once

class Queue;

const float qPriorities[8] = { 0.9f, 0.3f, 0.8f, 0.2f, 0.7f, 0.1f, 0.85f, 0.65f };

class GPU : public WITE::GPU {
public:
  GPU(VkPhysicalDevice device, size_t idx, size_t neededQueueCount = 0, const unsigned int* neededQueues = NULL);
  ~GPU();
  std::vector<WITE::Window*> windows;
  size_t idx;
  size_t getIdx() { return idx; };
  VkPhysicalDevice phys;
  unsigned int queueFamilyCount;
  VkQueueFamilyProperties queueFamProps[MAX_QUEUES];
  VkPhysicalDeviceProperties props;
  VkPhysicalDeviceMemoryProperties memProps;
  VkDevice device;
  uint32_t queueCount;
  VkPipelineCache pipeCache;
  //TODO usage tracker for load balancer
  union {
    struct { Queue *graphicsQ, *computeQ, *transferQ; };//Use compute for raytracing (if any)
    Queue* queues[MAX_QUEUES];
  };
  Queue* presentQ = NULL;//populated by the first window that finds one, tried by future windows before exploring other queue options
private:
};

