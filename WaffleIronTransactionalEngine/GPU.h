#pragma once

#include "stdafx.h"
#include "Globals.h"
#include "Queue.h"

const float qPriorities[8] = { 0.9f, 0.3f, 0.8f, 0.2f, 0.7f, 0.1f, 0.85f, 0.65f };

class GPU {
public:
  GPU(VkPhysicalDevice device, size_t idx, size_t neededQueueCount = 0, const unsigned int* neededQueues = NULL);
  ~GPU();
  std::vector<VWindow*> windows;
  size_t idx;
  VkPhysicalDevice phys;
  unsigned int queueFamilyCount;
  VkQueueFamilyProperties queueFamProps[MAX_QUEUES];
  VkPhysicalDeviceProperties props;
  VkPhysicalDeviceMemoryProperties memProps;
  VkDevice device;
  uint32_t queueCount;
  //TODO usage tracker for load balancer
  union {
    struct { VQueue *graphicsQ, *computeQ, *transferQ; };//Use compute for raytracing (if any)
    VQueue* queues[MAX_QUEUES];
  };
  VQueue* presentQ;//populated by the first window that finds one, tried by future windows before exploring other queue options
private:
};

template<class T, class D = default_delete<T>> class GPUResource {
public:
  typedef Callback_t<std::unique_ptr<T, D>, GPU*>* constructor;
  T* get(GPU* g) {
    if (data.capacity() < g.idx || !data[g->idx]) {
      ScopeLock lock(&allocLock);
      data.reserve(g.idx + 1);
      data[g->idx] = initer->call(g);
    }
    return data[g->idx].get();
  }
  inline T& operator[](std::ptrdiff_t idx) {
    if (data.capacity() < g.idx || !data[g->idx]) {
      ScopeLock lock(&allocLock);
      data.reserve(g.idx + 1);
      data[g->idx] = initer->call(g);
    }
    return *data[idx];//??
  }
  GPUResource(constructor c) : data(3), initer(c) {}
  ~GPUResource() = default;
private:
  std::vector<std::unique_ptr<T, D>> data;
  constructor initer;
  static SyncLock allocLock;
};

