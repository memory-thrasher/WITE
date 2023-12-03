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
