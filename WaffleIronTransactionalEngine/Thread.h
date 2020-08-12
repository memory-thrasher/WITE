#pragma once

#include "SyncLock.h"
#include "exportTypes.h"

namespace WITE {

  class Thread;
  //for when each thread needs its own instance of something
  template<class T> class ThreadResource {
  public:
    typedef std::shared_ptr<T> Tentry;
    typedefCB(Initer, Tentry)
    //typedef WITE::CallbackFactory<__VA_ARGS__> Initer_F; typedef typename Initer_F::callback_t Initer ;
    typedefCB(Destroyer, void, Tentry);
    template<typename U = T, std::enable_if_t<std::is_default_constructible<U>::value, int> = 0>
    ThreadResource() : ThreadResource(Initer_F::make(&std::make_shared)) {};
    ThreadResource(Initer typeInit, Destroyer destroyer = NULL) : typeInit(typeInit), typeDestroy(destroyer) {};
    ~ThreadResource() {
      Tentry* list;
      size_t count, i;
      if (typeDestroy) {
	count = listAll(&list);
	for (i = 0;i < count;i++)
	  if (data[i])
	    typeDestroy->call(data[i]);
      }
    }
    static Tentry makeEmpty() {
      return std::shared_ptr<T>(static_cast<T*>(malloc(sizeof(T))));
    }
    std::shared_ptr<T> get();
    std::shared_ptr<T> get(uint32_t tid) {
	  ScopeLock contextHold(&lock);
	  while (data.size() <= tid)
		data.emplace_back(std::nullptr_t());
      if (!data[tid]) data[tid] = typeInit->call();
      return data[tid];
    }
    size_t listAll(Tentry** pntr) {
	  ScopeLock contextHold(&lock);//yes, this is necessary
      *pntr = data.data();
      return data.size();
    }
    size_t size() {
	  ScopeLock contextHold(&lock);
      return data.size();
    }
  private:
    Initer typeInit;
    Destroyer typeDestroy;
    std::vector<Tentry> data;
    class SyncLock lock;
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

  template<class T> inline std::shared_ptr<T> ThreadResource<T>::get() {
    return get(Thread::getCurrentTid());
  };

};

