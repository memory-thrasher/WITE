/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <deque>
// #include <list>
#include <stack>

#include "syncLock.hpp"

namespace WITE {

  template<typename T, bool doLock = true> class recyclingPool {
  private:
    syncLock lock;
    std::deque<T> store;
    std::stack<T*> available;
  public:

    recyclingPool(size_t initialSize) :
      lock(),
      store(initialSize)
    {
      WITE::scopeLock locked(&lock);
      for(size_t i = 0;i < initialSize;i++)
	available.push(&store[i]);
    };

    recyclingPool() : recyclingPool(0) {};

    T* allocate() {
      WITE::scopeLock locked(&lock);
      T* ret;
      if(available.size()) {
	ret = available.top();
	available.pop();
      } else {
	ret = &store.emplace_back();
      }
      return ret;
    };

    void free(T* t) {
      WITE::scopeLock locked(&lock);
      available.push(t);
    };

  };

  template<typename T> class recyclingPool<T, false> {
  private:
    std::deque<T> store;
    std::stack<T*> available;
  public:

    recyclingPool(size_t initialSize) :
      store(initialSize)
    {
      for(size_t i = 0;i < initialSize;i++)
	available.push(&store[i]);
    };

    recyclingPool() : recyclingPool(0) {};

    T* allocate() {
      T* ret;
      if(available.size()) {
	ret = available.top();
	available.pop();
      } else {
	ret = &store.emplace_back();
      }
      return ret;
    };

    void free(T* t) {
      available.push(t);
    };

  };

}
