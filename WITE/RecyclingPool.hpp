#include <deque>
// #include <list>
#include <stack>

#include "SyncLock.hpp"

namespace WITE::Collections {

  template<typename T, bool doLock = true> class RecyclingPool {
  private:
    Util::SyncLock lock;
    std::deque<T> store;
    std::stack<T*> available;
    // std::list<T*> allocated;//TODO another class that is iterable
  public:

    RecyclingPool(size_t initialSize) :
      lock(),
      store(initialSize)// ,
      // available(std::deque<size_t>(initialSize))
    {
      WITE::Util::ScopeLock locked(&lock);
      for(size_t i = 0;i < initialSize;i++)
	available.push(&store[i]);
    };

    RecyclingPool() : RecyclingPool(0) {};

    T* allocate() {
      WITE::Util::ScopeLock locked(&lock);
      T* ret;
      if(available.size()) {
	ret = available.top();
      } else {
	ret = &store.emplace_back();
      }
      // allocated.push_front(ret);
      return ret;
    };

    void free(T* t) {
      WITE::Util::ScopeLock locked(&lock);
      available.push(t);
      // allocated.remove(t);
    };

    // allocated::iterator begin() {
    //   return allocated.begin();
    // };

    // allocated::iterator end() {
    //   return allocated.end();
    // };

  };

  template<typename T> class RecyclingPool<T, false> {
  private:
    std::deque<T> store;
    std::stack<T*> available;
  public:

    RecyclingPool(size_t initialSize) :
      store(initialSize)
    {
      for(size_t i = 0;i < initialSize;i++)
	available.push(&store[i]);
    };

    RecyclingPool() : RecyclingPool(0) {};

    T* allocate() {
      T* ret;
      if(available.size()) {
	ret = available.top();
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
