#pragma once

#include <array>

#include "FrameCounter.hpp"
#include "Callback.hpp"
#include "StdExtensions.hpp"

namespace WITE::Util {

  template<class T, size_t N = 2> class FrameSwappedResource {
  public:
    typedefCB(onFrameSwitch_cb, void, T&, T&);
  private:
    static_assert(N > 0);
    std::array<T, N> data;
    onFrameSwitch_cb onSwitch;
    std::atomic_uint64_t lastFrameFetch, lastFrameRelease;
  public:
    FrameSwappedResource(const std::array<T, N>& data, onFrameSwitch_cb onSwitch) : data(data), onSwitch(onSwitch) {}
    FrameSwappedResource(T* data, onFrameSwitch_cb onSwitch = NULL) : onSwitch(onSwitch) {
      memcpy(this->data.data(), data, sizeof(T) * N);
    }
    FrameSwappedResource(const std::initializer_list<T> data, onFrameSwitch_cb onSwitch) : data(data), onSwitch(onSwitch) {}
    FrameSwappedResource(onFrameSwitch_cb onSwitch) : data(), onSwitch(onSwitch) {}
    FrameSwappedResource(const FrameSwappedResource&) = delete;
    size_t getIdx(ssize_t offset = 0) {
      auto frame = FrameCounter::getFrame();
      if(onSwitch) {
	if(lastFrameFetch.exchange(frame) != frame) {
	  onSwitch(getRead(), getWrite());
	  lastFrameRelease = frame;
	} else
	  while(lastFrameRelease > frame);
      }
      size_t idx = frame + offset;
      if(idx < 0) [[unlikely]] idx = N + idx % N;
      else if(idx > (N << 1)) [[unlikely]] idx = idx % N;
      else if(idx > N) [[unlikely]] idx -= N;
      return idx;
    };
    size_t getWriteIdx() { return getIdx(0); ;}
    size_t getReadIdx() { return getIdx(1); };
    T& get(ssize_t offset = 0) {
      return data[getIdx(offset)];
    }
    inline T& getWrite() { return get(0); };
    inline T& getRead() { return get(1); };
    inline std::array<T, N>& all() { return data; };
    inline const std::array<const T, N>& call() const { return data; };
  };

}
