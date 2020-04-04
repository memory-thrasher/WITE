#pragma once

#include "SyncLock.h"

namespace WITE {

  template<class RET, class... RArgs> class Callback_t;
  
  class Thread;
  //for when each thread needs its own instance of something
  template<class T> class ThreadResource {
  public:
    typedef struct {
      bool exists;
      T obj;
    } Tentry;
    typedef Callback_t<void, T*>* Initer;
    ThreadResource(Initer typeInit = NULL, Initer destroyer = NULL) : typeInit(typeInit), typeDestroy(destroyer) {};
    ~ThreadResource() {
      Tentry* list;
      size_t count, i;
      if (typeDestroy) {
	count = listAll(&list)
	  for (i = 0;i < count;i++)
	    if (data[i].exists)
	      typeDestroy(&data[i].obj);
      }
    }
    T* get(uint32_t tid = Thread::getCurrentTid()) {
      size_t inited, count;
      if (data.size() <= tid) {
	ScopeLock contextHold(&lock);
	inited = data.size();
	if (inited <= tid) {
	  count = tid + 1;
	  data.reserve(count);//TODO pad?
	  memset(data.data() + inited * sizeof(Tentry), 0, (count - inited) * sizeof(Tentry));
	}
      }
      if (!data[tid].exists) {
	if(typeInit) typeInit(&data[tid].obj, arg);
	data[tid].exists = true;
      }
      return &data[tid].obj;
    }
    size_t listAll(Tentry** pntr) {
      *pntr = data.data();
      return data.size();
    }
    size_t size() {
      return data.size();
    }
  private:
    Initer typeInit, typeDestroy;
    volatile std::vector<Tentry> data;
    class SyncLock lock;
  };

  class Thread {
  public:
    typedef Callback_t<void>* threadEntry_t;
    static uint32_t getCurrentTid();//returns engine thread index, NOT system tid. Can be used to index.
    static void init();
    static void initThisThread(threadEntry_t entry = NULL);
    static void spawnThread(threadEntry_t entry);
    static Thread* current();
    uint32_t getTid();
    ~Thread();
  private:
    static std::atomic_uint32_t seed;
    static ThreadResource<Thread*> threads;
    threadEntry_t entry;
    uint32_t tid;
    Thread(threadEntry_t entry, uint32_t id);
  };

}
