#pragma once

namespace WITE {

  //for when each thread needs its own instance of something
  template<class T> class ThreadResource {
  public:
    typedef T* Tentry;
    typedefCB(Initer, Tentry)
    typedefCB(Destroyer, void, Tentry);
    template<typename U = T, std::enable_if_t<std::is_default_constructible<U>::value, int> = 0>
    ThreadResource() : ThreadResource(Initer_F::make(&makeDefault)) {};
    ThreadResource(Initer typeInit, Destroyer destroyer = NULL) : typeInit(typeInit), typeDestroy(destroyer), maxCreated(0) {
      memset(data, 0, sizeof(data));
    };
    ~ThreadResource() {
      ScopeLock contextHold(&lock);
      size_t i;
      for (i = 0;i < maxCreated;i++)
	if (data[i]) {
	  if(typeDestroy)
            typeDestroy->call(data[i]);
          delete data[i];
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
    T* get() {
      return get(Thread::getCurrentTid());
    }
    T* get(uint32_t tid) {
      if(tid >= maxCreated || !data[tid]) {
        ScopeLock contextHold(&lock);
        if(tid >= maxCreated)
          maxCreated = tid + 1;
        if(typeInit && !data[tid])
          data[tid] = typeInit->call();
      }
      return data[tid];
    }
    size_t listAll(Tentry* out, size_t maxOut) {
      size_t i, count = 0;
      for(i = 0;i < maxCreated && count < maxOut;i++) {
        if(data[i]) {
          out[count++] = data[i];
        }
      }
      return count;
    }
  private:
    Initer typeInit;
    Destroyer typeDestroy;
    Tentry data[MAX_THREADS];
    size_t maxCreated;
    SyncLock lock;
  };

  class Thread {
  public:
    typedefCB(threadEntry_t, void);
    static uint32_t getCurrentTid();//returns engine thread index, NOT system tid. Can be used to index.
    static void init();
    static void initThisThread(threadEntry_t entry = NULL);
    static void spawnThread(threadEntry_t entry);
    static Thread* current();
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

};

