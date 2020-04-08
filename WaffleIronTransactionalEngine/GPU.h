#pragma once

#include "stdafx.h"
#include "Globals.h"

class Queue;

const float qPriorities[8] = { 0.9f, 0.3f, 0.8f, 0.2f, 0.7f, 0.1f, 0.85f, 0.65f };

class GPU {
public:
  GPU(VkPhysicalDevice device, size_t idx, size_t neededQueueCount = 0, const unsigned int* neededQueues = NULL);
  ~GPU();
  std::vector<WITE::Window*> windows;
  size_t idx;
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
  Queue* presentQ;//populated by the first window that finds one, tried by future windows before exploring other queue options
private:
};

template<class T, class D = std::default_delete<T>> class GPUResource {//mostly thread safe, dirty read possible, most gpu stuff is kept on main thread
public:
  typedefCB(constructor, std::unique_ptr<T, D>, GPU*)
  T* get(GPU* g) {
    if (data.capacity() < g->idx || !data[g->idx]) {
      WITE::ScopeLock lock(&allocLock);
      data.reserve(g->idx + 1);
      if(!data[g->idx]) data[g->idx] = initer->call(g);
    }
    return data[g->idx].get();
  }
  T* get(size_t idx) {
    if (data.capacity() < idx || !data[idx]) {
      WITE::ScopeLock lock(&allocLock);
      data.reserve(idx + 1);
      if(!data[idx]) data[idx] = initer->call(vkSingleton.gpus[idx]);
    }
    return data[idx].get();
  }
  //template<class U = T> std::enable_if_t<std::is_copy_constructible<U>::value, T> inline operator[](size_t idx) { return *get(idx); };
  //template<class U = T> std::enable_if_t<std::is_array<U>::value, U> inline operator[](size_t idx) { return *get(idx); };
  GPUResource(constructor c) : data(3), initer(c) {}
  ~GPUResource() = default;
private:
  std::vector<std::unique_ptr<T, D>> data;
  constructor initer;
  static WITE::SyncLock allocLock;
};

template<class U, class Deleter> class GPUResource<U[], Deleter> {
public:
  typedefCB(constructor, std::unique_ptr<U[], Deleter>, GPU*)
  U* get(size_t idx);
  U* get(GPU* g);
  //template<class V = U, std::enable_if_t<std::is_copy_constructible<V>::value, int> = 0>
  inline U* operator[](size_t idx) {
    if (data.capacity() < idx || !data[idx]) {
      WITE::ScopeLock lock(&allocLock);
      data.reserve(idx + 1);
      if(!data[idx]) data[idx] = initer->call(vkSingleton.gpus[idx]);
    }
    return reinterpret_cast<U*>(data[idx].get());
  };
  GPUResource(constructor c) : data(3), initer(c) {}
  ~GPUResource() = default;
private:
  std::vector<std::unique_ptr<U[], Deleter>> data;
  constructor initer;
  static WITE::SyncLock allocLock;
};

