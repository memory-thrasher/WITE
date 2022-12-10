#include <deque>
// #include <list>
#include <stack>

#include "SyncLock.hpp"

namespace WITE::Collections {

  template<typename T, bool doLock> class RecyclingPool {
  private:
    Util::SyncLock lock;//TODO only if doLock
    std::deque<T> store;
    std::stack<T*> available;
    // std::list<T*> allocated;//TODO another class that is iterable
  public:

    RecyclingPool(size_t initialSize) :
      lock(),
      store(initialSize)// ,
      // available(std::deque<size_t>(initialSize))
    {
      WITE::Util::ScopeLock lock(doLock ? lock : NULL);
      for(size_t i = 0;i < initialSize;i++)
	available.push(&store[i]);
    };

    RecyclingPool() : RecyclingPool(0) {};

    T* allocate() {
      WITE::Util::ScopeLock lock(doLock ? lock : NULL);
      T* ret;
      if(available.size()) {
	ret = available.top();
      } else {
	ret = store.emplace_back();
      }
      // allocated.push_front(ret);
      return ret;
    };

    void free(T* t) {
      WITE::Util::ScopeLock lock(doLock ? lock : NULL);
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

}
