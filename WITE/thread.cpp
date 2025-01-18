/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#ifndef iswindows //*nix way to get cpu core count (guess)
#include <unistd.h>
#include <sys/sysinfo.h>
#else
#include <windows.h>
#endif

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
    counter++;
  };

  int32_t thread::guessCpuCount() {//static
    constexpr int64_t MIN = 4;//mostly to interpret 0 or -1 as failure
    int64_t ret = MIN;
#ifndef iswindows
    ret = sysconf(_SC_NPROCESSORS_CONF);
    if(ret <= MIN) ret = get_nprocs_conf();
    if(ret <= MIN) ret = sysconf(_SC_NPROCESSORS_ONLN);
    if(ret <= MIN) ret = get_nprocs();
#else
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    ret = sysinfo.dwNumberOfProcessors;
#endif
    if(ret <= MIN) ret = std::thread::hardware_concurrency();
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
    else
      WARN("Not joining thread because it is not joinable");
  };

}
