#pragma once

#include "stdafx.h"
#include "Globals.h"

class Queue;

const float qPriorities[8] = { 0.9f, 0.3f, 0.8f, 0.2f, 0.7f, 0.1f, 0.85f, 0.65f };

class GPU : public WITE::GPU {
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
  Queue* presentQ = NULL;//populated by the first window that finds one, tried by future windows before exploring other queue options
private:
};

template<class T, class D = std::default_delete<T>> class GPUResource {//mostly thread safe, dirty read possible, most gpu stuff is kept on main thread
private:
  void inline ensureExists(GPU* g) {
    size_t idx = g->idx;
    if(data.size() <= idx) {
      WITE::ScopeLock lock(&allocLock);
      //data.reserve(idx + 1);
      while(data.size() <= idx) data.emplace_back(std::nullptr_t());
      if(!data[idx]) data[idx] = initer->call(g);
    }
  };
  void inline ensureExists(size_t idx) {
    GPU* g = vkSingleton.gpus[idx];
    if(data.size() <= idx) {
      WITE::ScopeLock lock(&allocLock);
      //data.reserve(idx + 1);
      while(data.size() <= idx) data.emplace_back(std::nullptr_t());
      if(!data[idx]) data[idx] = initer->call(g);
    }
  };
public:
  typedef std::unique_ptr<T, D> UniqPtr;
  typedefCB(constructor, UniqPtr, GPU*)
  T* get(GPU* g) {
    ensureExists(g);
    return data[g->idx].get();
  }
  T* get(size_t idx) {
    ensureExists(idx);
    return data[idx].get();
  }
  template<class U = T>
  typename std::enable_if<std::is_array<U>::value, typename WITE::remove_array<U>::type>::type&
  get(GPU* gpu, size_t idx) {
    ensureExists(gpu);
    return data[gpu->idx][idx];
  }
  template<class U = T>
  typename std::enable_if<std::is_array<U>::value, typename WITE::remove_array<U>::type>::type&
    get(size_t gpu, size_t idx) {
    ensureExists(gpu);
    return data[gpu][idx];
  }
  template<class U = T>
  typename std::enable_if<std::is_array<U>::value, typename WITE::remove_array<U>::type>::type*
    getPtr(size_t gpu, size_t idx) {
    ensureExists(gpu);
    return &data[gpu][idx];
  }
  T* operator[](size_t idx) {
    ensureExists(idx);
    return data[idx].get();
  }
  //template<class U = T> std::enable_if_t<std::is_copy_constructible<U>::value, T> inline operator[](size_t idx) { return *get(idx); };
  //template<class U = T> std::enable_if_t<std::is_array<U>::value, U> inline operator[](size_t idx) { return *get(idx); };
  GPUResource(constructor c) : data(), initer(c) {}
  ~GPUResource() {
    WITE::ScopeLock lock(&allocLock);
    for(size_t i = 0;i < data.size();i++) {
      if(data[i]) {
        auto old = data[i].release();
        delete old;
      }
    }
    data.~vector();
    delete initer;
    //allocLock.~SyncLock();//does nothing
  };
private:
  std::vector<UniqPtr> data;
  constructor initer;
  WITE::SyncLock allocLock;
};

