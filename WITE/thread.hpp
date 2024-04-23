#pragma once

#include "syncLock.hpp"
#include "callback.hpp"
#include "stdExtensions.hpp"
#include "constants.hpp"
#include "DEBUG.hpp"

namespace WITE {

  template<class T> class threadResource {
  public:
    typedef T* Tentry;
    typedefCB(Initer, Tentry)
    typedefCB(Destroyer, void, Tentry);
    template<typename U = T, std::enable_if_t<std::is_default_constructible<U>::value, int> = 0>
    threadResource() : threadResource(Initer_F::make(&makeDefault)) {};
    threadResource(Initer typeInit, Destroyer destroyer = NULL) : typeInit(typeInit), typeDestroy(destroyer) {
      WITE::memset(data, 0, sizeof(data));
    };
    ~threadResource() {
      scopeLock contextHold(&lock);
      size_t i;
      for (i = 0;i < MAX_THREADS;i++)
	if (data[i]) {
	  if(typeDestroy)
            typeDestroy->call(data[i]);
          delete data[i];
	  data[i] = NULL;
        }
    }
    template<typename U = T, std::enable_if_t<std::is_default_constructible<U>::value, int> = 0>
    static Tentry makeDefault() {
      return new T();
    }
    static Tentry makeEmpty() {
      auto ret = static_cast<T*>(malloc(sizeof(T)));
      memset(ret, 0, sizeof(T));
      return ret;
    }
    static Initer initAsEmpty() {
      return Initer_F::make(&makeEmpty);
    }
    T* get();
    T* get(uint32_t tid) {
      ASSERT_TRAP(tid < MAX_THREADS, "threaded resource overflow");
      if(typeInit && !data[tid]) {
	scopeLock contextHold(&lock);
        if(typeInit && !data[tid])
          data[tid] = typeInit->call();
      }
      return data[tid];
    }
    T* getIfExists(size_t tid) {
      ASSERT_TRAP(tid < MAX_THREADS, "threaded resource overflow");
      return data[tid];
    }
    size_t listAll(Tentry* out, size_t maxOut) {
      size_t i, count = 0;
      for(i = 0;i < MAX_THREADS && count < maxOut;i++)
        if(data[i])
          out[count++] = data[i];
      return count;
    }
    T reduce(callbackPtr<T, const T&, const T&> cb) {
      T ret;
      if(typeInit) {
	auto pre = typeInit();
	ret = *pre;
	free(pre);
      }
      for(size_t i = 0;i < MAX_THREADS;i++)
	if(data[i])
	  ret = cb(ret, *data[i]);
      return ret;
    }
    void each(callbackPtr<void, T&> cb) {
      for(size_t i = 0;i < MAX_THREADS;i++)
	if(data[i])
	  cb(*data[i]);
    }
  private:
    Initer typeInit;
    Destroyer typeDestroy;
    Tentry data[MAX_THREADS];
    syncLock lock;
  };

  class thread {
  public:
    typedefCB(threadEntry_t, void);
    static uint32_t getCurrentTid();//returns engine thread index, NOT system tid. Can be used to index.
    static void init();
    static void initThisThread(threadEntry_t entry = NULL);
    static thread* spawnThread(threadEntry_t entry);//all spawned threads should be joined
    static thread* current();
    static thread* get(uint32_t tid);
    static void sleep(uint64_t ms = 0);
    static void sleepShort();//for non-busy wait, wait aa very small amount of time
    static void sleepShort(uint32_t& counter, uint32_t busyCount = 2048);//dynamically shifts from busy to non-busy wait based on iteration count
    static int32_t guessCpuCount();
    //^^todo maybe like std::this_thread::sleep_for(std::chrono::micorseconds(10));
    uint32_t getTid();
    void join();//all spawned threads should be joined
    ~thread();
    threadEntry_t entry;
  private:
    static std::atomic<uint32_t> seed;
    static threadResource<thread> threads;
    uint32_t tid;
    void* pthread;//void* to keep the pthread include out of the WITE headers just in case another platform needs something else
    thread(threadEntry_t entry, uint32_t id);
    thread();//dumby for allocation
  };

  template<class T> T* threadResource<T>::get() {
    return get(thread::getCurrentTid());
  }

}
