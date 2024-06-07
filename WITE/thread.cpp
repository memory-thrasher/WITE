#include <unistd.h>
#include <sys/sysinfo.h>
#include <chrono>

#include "thread.hpp"
#include "DEBUG.hpp"

namespace WITE {

  tid_t tid_none() {
    return std::thread::id();
    //std::thread::id specifies that default constructed is never a valid thread id, and is the default id for thread objects that do not represent real threads
  };

  threadResource<thread> thread::threads;//static

  tid_t thread::getCurrentTid() {//static
    return std::this_thread::get_id();
  };

  void thread::init() {//static
    //this did something back in the pthread days
    initThisThread();
  };

  void thread::initThisThread() {//static
    threads.get()->tid = getCurrentTid();
  };

  thread* thread::spawnThread(threadEntry_t entry) {//static
    std::atomic_uint8_t sem = 0;
    std::thread* baby = new std::thread([entry, &sem](){
      {
	uint32_t counter = 0;
	while(sem == 0) sleepShort(counter);
	sem = 2;
      }//parent has finished prepping ret. Also sem becomes invalid here
      initThisThread();
      if(entry) [[likely]] entry();
    });
    thread* ret = threads.get(baby->get_id());
    ret->threadObj.reset(baby);
    sem = 1;//done preparing baby thread's home
    uint32_t counter = 0;
    while(sem != 2) sleepShort(counter);//don't return until we know the baby thread is done with sem
    return ret;
  };

  thread* thread::current() {//static
    return threads.get();
  };

  thread* thread::get(tid_t tid) {//static
    return threads.get(tid);
  };

  void thread::sleep(uint64_t ns) {//static
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
  };

  void thread::sleepSeconds(float s) {//static
    std::this_thread::sleep_for(std::chrono::nanoseconds((uint64_t)(s * 1000000000)));
  };

  void thread::sleepShort() {//static
    std::this_thread::yield();
  };

  void thread::sleepShort(uint32_t& counter, uint32_t busyCount) {//static
    //test case is 15% faster with the limited busy wait
    if(counter >= busyCount) [[unlikely]]
      sleepShort();
    else
      counter++;
  };

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

  tid_t thread::getTid() {
    return tid;
  };

  void thread::join(){
    if(threadObj && threadObj->joinable())
      threadObj->join();
  };

}
