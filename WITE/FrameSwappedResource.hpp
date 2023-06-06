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
    Collections::CopyableArray<T, N> data;
    onFrameSwitch_cb onSwitch;
    std::atomic_uint64_t lastFrameFetch, lastFrameRelease;
  public:
    FrameSwappedResource(const Collections::CopyableArray<T, N>& data, onFrameSwitch_cb onSwitch) :
      data(data), onSwitch(onSwitch) {}
    FrameSwappedResource(T* data, onFrameSwitch_cb onSwitch = NULL) : onSwitch(onSwitch) {
      cpy(this->data.ptr(), data, N);
    }
    FrameSwappedResource(const std::initializer_list<T> data, onFrameSwitch_cb onSwitch = NULL) :
      data(data), onSwitch(onSwitch) {}
    FrameSwappedResource(onFrameSwitch_cb onSwitch) : data(), onSwitch(onSwitch) {}
    FrameSwappedResource(const FrameSwappedResource&) = delete;
    FrameSwappedResource() {}
    template<class... Args> requires requires(Args... args) { T(std::forward<Args>(args)...); }
    FrameSwappedResource(Args... args) : data(std::forward<Args>(args)...) {};//construct each element with same arguments

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
      if(idx < 0) [[unlikely]] idx = (N + idx) % N;
      else if(idx >= (N << 1)) [[unlikely]] idx = idx % N;
      else if(idx >= N) [[unlikely]] idx -= N;
      return idx;
    };
    size_t getWriteIdx() { return getIdx(0); ;}
    size_t getReadIdx() { return getIdx(1); };
    T& get(ssize_t offset = 0) {
      return data[getIdx(offset)];
    }
    inline T& getWrite() { return get(0); };
    inline T& getRead() { return get(1); };
    inline Collections::CopyableArray<T, N>& all() { return data; };
    inline const Collections::CopyableArray<const T, N>& call() const { return data; };

  };

  // template<class T, size_t L, size_t N = 2>
  // inline FrameSwappedResource<T*[L], N> DeMultiplex(FrameSwappedResource<T, N>*(&multis)[L]) {
  //   Collections::CopyableArray<T*[L], N> ret;
  //   for(size_t i = 0;i < L;i++)
  //     for(size_t j = 0;j < N;j++)
  // 	ret[j][i] = &multis[i].all()[j];
  //   return { ret, NULL };
  // };

}
