#pragma once

#include <vector>
#include <map>

#include "FrameCounter.hpp"
#include "Thread.hpp"

namespace WITE::Collections {

  template<class K, class V> class FrameBufferedMap {
  public:
    typedef std::pair<const K, V> T;
  private:
    struct bucket_t {
      std::vector<T> toBeAdded;
      std::vector<K> toBeRemoved;
    };
    Platform::ThreadResource<bucket_t> buckets;
    std::map<K, V> live;
    std::atomic_uint64_t lastFrameFetch, lastFrameRelease;
    void applyNow(bucket_t& b) {
      for(K k : b.toBeRemoved)
	live.erase(k);
      live.insert(b.toBeAdded.begin(), b.toBeAdded.end());
      b.toBeAdded.clear();
      b.toBeRemoved.clear();
    };
    Util::CallbackPtr<void, bucket_t&> applyNow_cb;
  public:
    FrameBufferedMap() :
      applyNow_cb(Util::CallbackFactory<void, bucket_t&>::make(this, &FrameBufferedMap::applyNow)) {};
    FrameBufferedMap(const FrameBufferedMap<K, V>& o) : live(o.live),
      applyNow_cb(Util::CallbackFactory<void, bucket_t&>::make(this, &FrameBufferedMap::applyNow)) {};
    void checkFrame(bool blockIfInProgress = true) {
      auto frame = Util::FrameCounter::getFrame();
      if(lastFrameFetch.exchange(frame) != frame) {
	buckets.each(applyNow_cb);
	lastFrameRelease = frame;
      } else if(blockIfInProgress)
	while(lastFrameRelease < frame);
    };
    void push(T gnu) {
      checkFrame();
      buckets.get()->toBeAdded.push_back(gnu);
    };
    inline void push(K k, V v) {
      push({k, v});
    };
    void remove(K doomed) {
      checkFrame();
      buckets.get()->toBeRemoved.push_back(doomed);
    };
    auto count() {
      checkFrame();
      return live.size();
    };
    auto begin() {
      checkFrame();
      return live.begin();
    };
    auto end() {
      checkFrame();
      return live.end();
    };
  };

};
