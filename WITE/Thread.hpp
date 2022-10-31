#pragma once

#include "SyncLock.hpp"
#include "Callback.hpp"
#include "StdExtensions.hpp"

namespace WITE::Platform {

  template<class T> class ThreadResource {
  public:
    static constexpr size_t MAX_THREADS = 256;
    typedef T* Tentry;
    typedefCB(Initer, Tentry)
    typedefCB(Destroyer, void, Tentry);
    template<typename U = T, std::enable_if_t<std::is_default_constructible<U>::value, int> = 0>
    ThreadResource() : ThreadResource(Initer_F::make(&makeDefault)) {};
    ThreadResource(Initer typeInit, Destroyer destroyer = NULL) : typeInit(typeInit), typeDestroy(destroyer) {
      WITE::memset(data, 0, sizeof(data));
    };
    ~ThreadResource() {
      Util::ScopeLock contextHold(&lock);
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
      if(!data[tid]) {
	Util::ScopeLock contextHold(&lock);
        if(typeInit && !data[tid])
          data[tid] = typeInit->call();
      }
      return data[tid];
    }
    T* getIfExists(size_t tid) {
      return data[tid];
    }
    size_t listAll(Tentry* out, size_t maxOut) {
      size_t i, count = 0;
      for(i = 0;i < MAX_THREADS && count < maxOut;i++)
        if(data[i])
          out[count++] = data[i];
      return count;
    }
    T reduce(Util::CallbackPtr<T, const T&, const T&> cb) {
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
    void each(Util::CallbackPtr<void, T&> cb) {
      for(size_t i = 0;i < MAX_THREADS;i++)
	if(data[i])
	  cb(*data[i]);
    }
  private:
    Initer typeInit;
    Destroyer typeDestroy;
    Tentry data[MAX_THREADS];
    Util::SyncLock lock;
  };

  class Thread {
  public:
    typedefCB(threadEntry_t, void);
    static uint32_t getCurrentTid();//returns engine thread index, NOT system tid. Can be used to index.
    static void init();
    static void initThisThread(threadEntry_t entry = NULL);
    static void spawnThread(threadEntry_t entry);
    static Thread* current();
    static void sleep(uint64_t ms = 0);
    static void sleepShort();//for non-busy wait, wait aa very small amount of time
    //^^todo maybe like std::this_thread::sleep_for(std::chrono::micorseconds(10));
    uint32_t getTid();
    ~Thread();
  private:
    static std::atomic<uint32_t> seed;
    static ThreadResource<Thread> threads;
    threadEntry_t entry;
    uint32_t tid;
    Thread(threadEntry_t entry, uint32_t id);
    Thread();//dumby for allocation
  };

  template<class T> T* ThreadResource<T>::get() {
    return get(Thread::getCurrentTid());
  }

}
