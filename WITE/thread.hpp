/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <thread>
#include <map>
#include <memory>

#include "syncLock.hpp"
#include "callback.hpp"
#include "stdExtensions.hpp"
#include "DEBUG.hpp"

//std::thread wrapper in case other platforms need something different
namespace WITE {

  typedef std::thread::id tid_t;
  tid_t tid_none();

  template<class T> class threadResource {
  public:
    typedef T* Tentry;
    typedefCB(Initer, Tentry);

    template<typename U = T, std::enable_if_t<std::is_default_constructible<U>::value, int> = 0>
    threadResource() : threadResource(Initer_F::make(&makeDefault)) {};
    threadResource(Initer typeInit) : typeInit(typeInit) {};

    template<typename U = T, std::enable_if_t<std::is_default_constructible<U>::value, int> = 0>
    static Tentry makeDefault() {
      return new T();
    };

    T* get();

    T* get(tid_t tid) {
      scopeLock contextHold(&lock);
      auto& d = data[tid];
      if(!d && typeInit)
	d.reset(typeInit());
      return d.get();
    };

    T* getIfExists(tid_t tid) {
      scopeLock contextHold(&lock);
      return data[tid].get();
    };

    size_t listAll(Tentry* out, size_t maxOut) {
      scopeLock contextHold(&lock);
      size_t count = 0;
      for(auto& pair : data)
	if(count < maxOut)
	  out[count++] = pair.second.get();
      return count;
    };

    void each(callbackPtr<void, T&> cb) {
      scopeLock contextHold(&lock);
      for(auto& pair : data)
	cb(pair.second.get());
    };

    auto inline begin() {
      return data.begin();
    };

    auto inline end() {
      return data.end();
    };

  private:
    Initer typeInit;
    std::map<tid_t, std::unique_ptr<T>> data;
    syncLock lock;
  };

  class thread {
  public:
    typedefCB(threadEntry_t, void);
    static tid_t getCurrentTid();//returns engine thread index, NOT system tid. Can be used to index.
    static void init();
    static void initThisThread();
    static thread* spawnThread(threadEntry_t entry);//all spawned threads should be joined
    static thread* current();
    static thread* get(tid_t tid);
    static void sleep(uint64_t ns = 0);
    static void sleepSeconds(float s);//mostly for test cases (more readable 10000000000 ns)
    static void sleepShort();//for non-busy wait, wait aa very small amount of time
    static void sleepShort(uint32_t& counter, uint32_t busyCount = 128);//dynamically shifts from busy to non-busy wait based on iteration count
    static int32_t guessCpuCount();
    tid_t getTid();
    void join();//all spawned threads should be joined
  private:
    static threadResource<thread> threads;
    tid_t tid;
    std::unique_ptr<std::thread> threadObj;//might be null, for main or threads created by other means
  };

  template<class T> T* threadResource<T>::get() {
    return get(thread::getCurrentTid());
  }

}
