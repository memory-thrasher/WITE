#pragma once

#include <deque>
#include <stack>
#include <vector>

#include "syncLock.hpp"

namespace WITE {

  template<class T> class iterableRecyclingPool {
  private:
    static_assert(std::is_default_constructible<T>::value);
    std::deque<T> store;
    std::stack<T*> available;
    std::vector<T*> allocated;
  public:

    iterableRecyclingPool(size_t initialSize) :
      store(initialSize)
    {
      for(size_t i = 0;i < initialSize;i++)
	available.push(&store[i]);
    };

    iterableRecyclingPool() : iterableRecyclingPool(0) {};

    T* allocate() {
      T* ret;
      if(available.size()) {
	ret = available.top();
	available.pop();
      } else {
	ret = &store.emplace_back();
      }
      allocated.push_back(ret);
      return ret;
    };

    void free(T* t) {
      available.push(t);
      remove(allocated, t);
    };

    inline auto begin() { return allocated.begin(); };
    inline auto end() { return allocated.end(); };

    inline auto begin() const { return allocated.cbegin(); };
    inline auto end() const { return allocated.cend(); };

  };

};
