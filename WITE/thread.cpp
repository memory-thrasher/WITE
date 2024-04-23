#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#include "thread.hpp"
#include "DEBUG.hpp"

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
      auto storage = reinterpret_cast<thread*>(param);
      putThreadRef(storage);
      if (storage->entry) storage->entry();
      return 0;
    }

    bool hasThreadRef() {
      return pthread_getspecific(threadInternal::threadObjKey);
    }

  }

  thread* thread::get(uint32_t tid) {
    return threads.get(tid);
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

  thread* thread::spawnThread(threadEntry_t entry) {//static
    pthread_t* temp = new pthread_t();
    uint32_t tid = seed.fetch_add(1, std::memory_order_relaxed);
    thread* t = threads.get(tid);
    new(t)thread(entry, tid);
    t->pthread = reinterpret_cast<void*>(temp);
    pthread_create(temp, NULL, &threadInternal::pthreadCallback, reinterpret_cast<void*>(t));
    return t;
  }

  void thread::join() {
    if(getCurrentTid() == tid) [[unlikely]] return;
    ASSERT_TRAP(pthread_join(*reinterpret_cast<pthread_t*>(pthread), NULL) == 0, "pthread_join failed");
  };

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

  void thread::sleepShort() {
    usleep(1);
  }

  void thread::sleepShort(uint32_t& counter, uint32_t busyCount) {
    if(++counter % busyCount == 0) [[unlikely]]
      usleep(1);
  }

  int32_t thread::guessCpuCount() {//static
    constexpr int64_t MIN = 4;//mostly to interpret 0 or -1 as failure
    int64_t ret;
    ret = sysconf(_SC_NPROCESSORS_CONF);
    if(ret <= MIN) ret = get_nprocs_conf();
    if(ret <= MIN) ret = sysconf(_SC_NPROCESSORS_ONLN);
    if(ret <= MIN) ret = get_nprocs();
    //TODO other methods to guess here
    if(ret <= MIN) ret = MIN;
    return int32_t(ret);
  };

}
