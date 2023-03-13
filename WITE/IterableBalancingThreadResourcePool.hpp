#pragma once

#include <deque>
#include <stack>

#include "SyncLock.hpp"
#include "Thread.hpp"
#include "LinkedTree.hpp"

namespace WITE::Collections {

  template<typename T, size_t bundleSize = 64> class IterableBalancingThreadResourcePool {
  private:
    typedef T bundle[bundleSize];
    struct scrap {
    private:
      T* data[bundleSize];
    public:
      scrap() {};
      scrap(const scrap& o) {
	memcpy(data, o.data, sizeof(data));
      };
      inline T*& operator[](int i) { return data[i]; };
    };
    Util::SyncLock commonsLock;
    //associated thread owns the store (and so can locklessly expand it) but NOT the contents
    Platform::ThreadResource<std::deque<bundle>> stores;
    Platform::ThreadResource<std::stack<T*>> available;
    std::stack<scrap> commonScrap;
    LinkedTree<T> commonAllocated;
    LinkedTree<T>::Iterator commonIterator;
  public:

    IterableBalancingThreadResourcePool() : commonIterator(commonAllocated.iterate()) {};

    T* allocate() {
      auto* avail = available.get();
      T* ret = NULL;
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
      commonAllocated.insert(ret);
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
	commonScrap.emplace(s);
      } else {
	avail->push(t);
      }
    };

    T* getAny() {
      Util::ScopeLock lock(&commonsLock);
      return commonIterator();
    };

  };

}
