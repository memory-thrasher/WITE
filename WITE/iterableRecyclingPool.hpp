#pragma once

#include <deque>
#include <stack>
#include <vector>

#include "syncLock.hpp"

namespace WITE {

  template<class T, class... Args> class iterableRecyclingPool {
  private:
    std::deque<T> store;
    std::stack<T*> available;
    std::vector<T*> allocated;
    const std::tuple<Args...> args;
  public:

    iterableRecyclingPool(Args... args) :
      args(std::forward<Args>(args)...)
    {};

    T* allocate() {
      T* ret;
      if(available.size()) [[likely]] {
	ret = available.top();
	available.pop();
      } else {
	typedef decltype(store) S;
	static constexpr T&(S::*emplaceMbrFn)(Args&&...) = &S::template emplace_back<Args...>;
	ret = &std::apply(emplaceMbrFn, std::tuple_cat(std::tie(store), args));
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
