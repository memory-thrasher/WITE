#include "Thread.hpp"
#include "DEBUG.hpp"

#include <pthread.h>
#include <unistd.h>

namespace WITE::Platform {

  //#if posix, but c should always be posix

  namespace ThreadInternal {

    pthread_key_t threadObjKey;

    void threadDestructorWrapper(void* thread) {
      //Thread* t = reinterpret_cast<Thread*>(thread);
      //TODO cleanup storage?
    }

    void putThreadRef(Thread* ref) {
      if (pthread_setspecific(threadObjKey, reinterpret_cast<void*>(ref)))
	CRASH("Thread init failed: Failed to store thread id in thread storage (pthread)\n");
    }

    void* pthreadCallback(void* param) {
      auto temp = reinterpret_cast<Thread::threadEntry_t*>(param);
      Thread::initThisThread(*temp);
      delete temp;
      return 0;
    }

    bool hasThreadRef() {
      return pthread_getspecific(ThreadInternal::threadObjKey);
    }

  }

  syncLock initLock;
  void Thread::init() {//static
    if (seed.load() == 0) {
      scopeLock lock(&initLock);
      if (seed.load() == 0) {
	if(pthread_key_create(&ThreadInternal::threadObjKey, &ThreadInternal::threadDestructorWrapper))
	  CRASH("Failed to allocate thread key. This should not happen.");
      }
    }
    initThisThread();
  }

  Thread* Thread::current() {
    void* ret = pthread_getspecific(ThreadInternal::threadObjKey);
    if (!ret) //CRASHRET_PREINIT(NULL, "Failed to fetch thread data by key. This should not happen.");
      return NULL;
    return static_cast<Thread*>(ret);
  }

  void Thread::spawnThread(threadEntry_t entry) {//static
    pthread_t temp;
    pthread_create(&temp, NULL, &ThreadInternal::pthreadCallback, reinterpret_cast<void*>(new threadEntry_t(entry)));
  }

  //#endif posix

  std::atomic<uint32_t> Thread::seed = 0;//static
  ThreadResource<Thread> Thread::threads(decltype(Thread::threads)::initAsEmpty());//static

  void Thread::initThisThread(threadEntry_t entry) {//static, entry defaults to null
    if(ThreadInternal::hasThreadRef()) {
      WARN("Repeated init on this thread");
      if(entry) entry->call();
      return;
    }
    uint32_t tid = seed.fetch_add(1, std::memory_order_relaxed);
    auto storage = threads.get(tid);
    new(storage)Thread(entry, tid);
    ThreadInternal::putThreadRef(storage);
    if (storage->entry) storage->entry();
  }

  uint32_t Thread::getTid() {
    return tid;
  }

  uint32_t Thread::getCurrentTid() {
    if (seed.load() == 0)
      return 0;
    auto c = current();
    return c ? c->getTid() : -5;
  }

  Thread::Thread(threadEntry_t entry, uint32_t id) : entry(entry), tid(id) {}
  Thread::Thread() {}
  Thread::~Thread() {}

  void Thread::sleep(uint64_t ms) { //static
    usleep(ms * 1000);
  }

  void Thread::sleepShort() { //static
    usleep(10);
  }

}
