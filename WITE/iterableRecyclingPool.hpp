/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

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
    std::tuple<Args...> args;
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
