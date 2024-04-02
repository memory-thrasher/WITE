#include "thread.hpp"
#include "DEBUG.hpp"

#include <pthread.h>
#include <unistd.h>

namespace WITE {

  //#if posix, but c should always be posix

  namespace threadInternal {

    pthread_key_t threadObjKey;

    void threadDestructorWrapper(void* thread) {
      //thread* t = reinterpret_cast<thread*>(thread);
      //TODO cleanup storage?
    }

    void putThreadRef(thread* ref) {
      if (pthread_setspecific(threadObjKey, reinterpret_cast<void*>(ref)))
	CRASH("Thread init failed: Failed to store thread id in thread storage (pthread)\n");
    }

    void* pthreadCallback(void* param) {
      auto temp = reinterpret_cast<thread::threadEntry_t*>(param);
      thread::initThisThread(*temp);
      delete temp;
      return 0;
    }

    bool hasThreadRef() {
      return pthread_getspecific(threadInternal::threadObjKey);
    }

  }

  syncLock initLock;
  void thread::init() {//static
    if (seed.load() == 0) {
      scopeLock lock(&initLock);
      if (seed.load() == 0) {
	if(pthread_key_create(&threadInternal::threadObjKey, &threadInternal::threadDestructorWrapper))
	  CRASH("Failed to allocate thread key. This should not happen.");
      }
    }
    initThisThread();
  }

  thread* thread::current() {
    void* ret = pthread_getspecific(threadInternal::threadObjKey);
    if (!ret) //CRASHRET_PREINIT(NULL, "Failed to fetch thread data by key. This should not happen.");
      return NULL;
    return static_cast<thread*>(ret);
  }

  void thread::spawnThread(threadEntry_t entry) {//static
    pthread_t temp;
    pthread_create(&temp, NULL, &threadInternal::pthreadCallback, reinterpret_cast<void*>(new threadEntry_t(entry)));
  }

  //#endif posix

  std::atomic<uint32_t> thread::seed = 0;//static
  threadResource<thread> thread::threads(decltype(thread::threads)::initAsEmpty());//static

  void thread::initThisThread(threadEntry_t entry) {//static, entry defaults to null
    if(threadInternal::hasThreadRef()) {
      WARN("Repeated init on this thread");
      if(entry) entry->call();
      return;
    }
    uint32_t tid = seed.fetch_add(1, std::memory_order_relaxed);
    auto storage = threads.get(tid);
    new(storage)thread(entry, tid);
    threadInternal::putThreadRef(storage);
    if (storage->entry) storage->entry();
  }

  uint32_t thread::getTid() {
    return tid;
  }

  uint32_t thread::getCurrentTid() {
    if (seed.load() == 0)
      return 0;
    auto c = current();
    return c ? c->getTid() : -5;
  }

  thread::thread(threadEntry_t entry, uint32_t id) : entry(entry), tid(id) {}
  thread::thread() {}
  thread::~thread() {}

  void thread::sleep(uint64_t ms) { //static
    usleep(ms * 1000);
  }

  void thread::sleepShort() { //static
    usleep(10);
  }

}
