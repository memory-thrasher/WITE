#include "Thread.hpp"
#include "DEBUG.hpp"

#include <pthread.h>
#include <unistd.h>

namespace WITE::Platform {

// #ifdef _WIN32//copied from old implementation, TODO test/update when win binaries are desirable if pthread doesn't work

//   DWORD dwTlsIndex;

//   void Thread::init() {//static
//     if (seed.load() == 0) {
//       dwTlsIndex = TlsAlloc();
//     }
//     initThisThread();
//   }

//   Thread* Thread::current() {
//     LPVOID ret = TlsGetValue(dwTlsIndex);
//     if (!ret) CRASHRET(NULL);
//     return static_cast<Thread*>(ret);
//   }

//   void putThreadRef(Thread* ref) {
//     if (!TlsSetValue(dwTlsIndex, ref)) CRASH("Thread init failed: Failed to store thread id in thread storage (win32 kit)\n");
//   }

//   DWORD WINAPI winThreadCallback(LPVOID lpParam) {
//     Thread::initThisThread(static_cast<Thread::threadEntry_t>(lpParam));
//     return 0;
//   }

//   void Thread::spawnThread(threadEntry_t entry) {//static
//     CreateThread(NULL, 0, winThreadCallback, entry, 0, NULL);
//   }

// #endif

  //#if posix, but c should always be posix

  namespace ThreadInternal {

    pthread_key_t threadObjKey;

    void threadDestructorWrapper(void* thread) {
      Thread* t = reinterpret_cast<Thread*>(thread);
      delete t;
    }

    void putThreadRef(Thread* ref) {
      if (pthread_setspecific(threadObjKey, reinterpret_cast<void*>(ref)))
	CRASH("Thread init failed: Failed to store thread id in thread storage (pthread)\n");
    }

    void* pthreadCallback(void* param) {
      Thread::initThisThread(reinterpret_cast<Thread::threadEntry_t>(param));
      return 0;
    }

  }

  void Thread::init() {//static
    if (seed.load() == 0) {
      if(pthread_key_create(&ThreadInternal::threadObjKey, &ThreadInternal::threadDestructorWrapper))
	CRASH("Failed to allocate thread key. This should not happen.");
    }
    initThisThread();
  }

  Thread* Thread::current() {
    void* ret = pthread_getspecific(ThreadInternal::threadObjKey);
    if (!ret) CRASHRET(NULL, "Failed to fetch thread data by key. This should not happen.");
    return static_cast<Thread*>(ret);
  }

  void Thread::spawnThread(threadEntry_t entry) {//static
    pthread_t temp;
    pthread_create(&temp, NULL, &ThreadInternal::pthreadCallback, reinterpret_cast<void*>(entry));
  }

  //#endif posix

  std::atomic<uint32_t> Thread::seed;//static
  ThreadResource<Thread> Thread::threads(decltype(Thread::threads)::initAsEmpty());//static

  void Thread::initThisThread(threadEntry_t entry) {//static, entry defaults to null
    uint32_t tid = seed.fetch_add(1, std::memory_order_relaxed);
    auto storage = threads.get(tid);
    new(storage)Thread(entry, tid);
    ThreadInternal::putThreadRef(storage);
    if (storage->entry) storage->entry->call();
  }

  uint32_t Thread::getTid() {
    return tid;
  }

  uint32_t Thread::getCurrentTid() {
    return current()->getTid();
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
