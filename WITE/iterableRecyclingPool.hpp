#pragma once

#include <deque>
#include <stack>
#include <vector>

#include "syncLock.hpp"

namespace WITE {

  template<class T, class... Args> class iterableRecyclingPool {
  private:
    static_assert(std::is_default_constructible<T>::value);
    std::deque<T> store;
    std::stack<T*> available;
    std::vector<T*> allocated;
    std::tuple<Args...> args;
  public:

    iterableRecyclingPool(Args... args, size_t initialSize = 0) :
      store(initialSize),
      args(std::forward<Args>(args)...)
    {
      for(size_t i = 0;i < initialSize;i++)
	available.push(&store[i]);
    };

    T* allocate() {
      T* ret;
      if(available.size()) [[likely]] {
	ret = available.top();
	available.pop();
      } else {
	ret = &store.emplace_back(std::forward<Args>(args)...);
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

    inline size_t size() const { return allocated.size(); };

  };

};
