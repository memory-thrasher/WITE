#pragma once

#include "Callback.hpp"
#include "DEBUG.hpp"
#include "types.hpp"

namespace WITE::GPU {

  //everything deals in literal T because T is almost certainly an opaque handle to a vulkan object.
  //T should always be a trivial and tiny. If that's not the case just make T be a pointer
  template<class T> class PerGpu {
  public:
    static_assert(sizeof(T) <= sizeof(T*) * 2);
    typedefCB(creator_t, T, size_t);
    typedefCB(destroyer_t, void, T&, size_t);
    typedef T value_t;
  private:
    T data[MAX_GPUS];
    deviceMask_t dataAllocationMask;
    creator_t creator;
    destroyer_t destroyer;
  public:
    PerGpu(creator_t creator, destroyer_t destroyer) : creator(creator), destroyer(destroyer) {};
    ~PerGpu() {
      if(destroyer)
	for(size_t i = 0;i < MAX_GPUS;i++)
	  if(dataAllocationMask & (1 << i))
	    destroyer(data[i], i);
    };
    T get(size_t idx) {
      ASSERT_TRAP(idx < MAX_GPUS, "out of bounds");
      T ret;
      if(dataAllocationMask & (1 << idx))
	ret = data[idx];
      else {
	ret = data[idx] = creator(idx);
	dataAllocationMask |= (1 << idx);
      }
      return ret;
    };
    inline T& getRef(size_t idx) {
      ASSERT_TRAP(idx < MAX_GPUS, "out of bounds");
      if(!(dataAllocationMask & (1 << idx))) {
	data[idx] = creator(idx);
	dataAllocationMask |= (1 << idx);
      }
      return data[idx];
    };
    inline T operator[](size_t i) { return get(i); };
    T set(size_t idx, T gnu) {//returns old data if any, caller is responsible for freeing it
      ASSERT_TRAP(idx < MAX_GPUS, "out of bounds");
      if(dataAllocationMask & (1 << idx)) {
	T ret = data[idx];
	data[idx] = gnu;
	return ret;
      } else {
	data[idx] = gnu;
	dataAllocationMask |= (1 << idx);
	return NULL;
      }
    };
  };

};

#include "MapPerGpu.hpp"

