#include "SyncLock.hpp"

namespace WITE::Collections {

  template<typename T, bool doLock> class RecyclingPool {
  private:
    SyncLock lock;//TODO only if doLock
    std::deque<T> store;
    std::stack<T*> free;
    std::list<T*> allocated;
  public:

    RecyclingPool(size_t initialSize) :
      lock(),
      store(initialSize),
      free(std::deque<size_t>(initialSize)) {
      WITE::Util::ScopeLock lock(doLock ? lock : NULL);
      for(size_t i = 0;i < initialSize;i++)
	free.push(&store[i]);
    };

    T* allocate() {
      WITE::Util::ScopeLock lock(doLock ? lock : NULL);
      T* ret;
      if(free.size()) {
	ret = free.top();
      } else {
	ret = store.emplace_back();
      }
      allocated.push_front(ret);
      return ret;
    };

    void free(T* t) {
      WITE::Util::ScopeLock lock(doLock ? lock : NULL);
      free.push(t);
      allocaated.remove(t);
    };

    allocated::iterator begin() {
      return allocated.begin();
    };

    allocated::iterator end() {
      return allocated.end();
    };

  };

}
