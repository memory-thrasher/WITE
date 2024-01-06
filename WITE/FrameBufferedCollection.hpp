#pragma once

#include <vector>

#include "FrameCounter.hpp"
#include "Thread.hpp"

namespace WITE::Collections {

  template<class T> class FrameBufferedCollection {
  private:
    struct bucket_t {
      std::vector<T> toBeAdded, toBeRemoved;
    };
    Platform::ThreadResource<bucket_t> buckets;
    std::vector<T> live;
    std::atomic_uint64_t lastFrameFetch, lastFrameRelease;
    void applyNow(bucket_t& b) {
      Collections::remove_if(live, [b](auto e){ return Collections::contains(b.toBeRemoved, e); });
      live.insert(live.end(), b.toBeAdded.begin(), b.toBeAdded.end());
      b.toBeAdded.clear();
      b.toBeRemoved.clear();
    };
    Util::CallbackPtr<void, bucket_t&> applyNow_cb;
  public:
    FrameBufferedCollection() :
      applyNow_cb(Util::CallbackFactory<void, bucket_t&>::make(this, &FrameBufferedCollection::applyNow)) {};
    FrameBufferedCollection(const FrameBufferedCollection<T>& o) : live(o.live),
      applyNow_cb(Util::CallbackFactory<void, bucket_t&>::make(this, &FrameBufferedCollection::applyNow)) {};
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
    void remove(T doomed) {
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
