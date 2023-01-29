#include <deque>
// #include <list>
#include <stack>

#include "SyncLock.hpp"
#include "Thread.hpp"

namespace WITE::Collections {

  template<typename T, size_t bundleSize = 64> class BalancingThreadResourcePool {
  private:
    typedef T[bundleSize] bundle;
    typedef T*[bundleSize] scrap;
    Util::SyncLock commonsLock;
    //associated thread owns the store (and so can locklessly expand it) but NOT the contents
    ThreadResource<std::deque<bundle>> stores;
    ThreadResource<std::stack<T*>> available;
    std::stack<T*> commonScrap;
    std::list<T*> commonAllocated;
  public:

    RecyclingPool() {};

    T* allocate() {
      auto* avail = available.get();
      T* ret;
      if(!avail->empty()) {
	[[likely]]
	ret = avail->top();
	avail->pop();
      }
      if(!ret) {
	Util::ScopeLock lock(&commonsLock);
	if(!commonScrap.empty()) {
	  scrap b = commonScrap.top();
	  commonScrap.pop();
	  lock.release();
	  ret = b[0];
	  for(size_t i = 1;i < bundleSize;i++)
	    avail->push(b[i]);
	}
      }
      if(!ret) {
	bundle& b = stores.get()->emplace_back();
	for(size_t i = 1;i < bundleSize;i++)
	  avail->push(&b[i]);
	ret = &b[0];
      }
      Util::ScopeLock lock(&commonsLock);
      commonAllocated.push_back(ret);
      return ret;
    };

    void free(T* t) {
      {
	Util::ScopeLock lock(&commonsLock);
	commonAllocated.remove(t);
      }
      auto* avail = available.get();
      if(avail->size() > bundleSize*2) {
	scrap s;
	s[0] = t;
	for(size_t i = 1;i < bundleSize;i++) {
	  s[i] = avail->top();
	  avail->pop();
	}
	Util::ScopeLock lock(&commonsLock);
	commonScrap->push_back(s);
      } else {
	avail->push_back(t);
      }
    };

    //only invalidated if the current element is dropped. Unclear what happens in that case, might need to implement a list with self-recovering iterator.
    auto begin() {
      Util::ScopeLock lock(&commonsLock);
      return allocated.cbegin();
    };

    auto end() {
      Util::ScopeLock lock(&commonsLock);
      return allocated.cend();
    };

  };

}
