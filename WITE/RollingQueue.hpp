#include "Thread.hpp"

namespace WITE::Collections {

  //thread safe and fast, well ordered fifo queue of fixed size.
  template<typename T, size_t SIZE> class RollingQueue {
  private:
    T data[SIZE];
    size_t nextIn,//the index of an empty datum that is next to be allocated
      nextOut;//the index of the next datum to be popped
    constexpr size_t wrap(size_t i) { return i > SIZE ? i > SIZE * 2 ? i % SIZE : i - SIZE : i; }
  public:

    //only blocks if strongly-after an unfinished call to the same method, blocks until that call completes
    T* push(T* src) {
      size_t target = nextIn,
	nextTarget = wrap(target + 1);
      //we always have 1 empty slot, or else we couldn't tell full from empty because lastIn == nextOut
      if(nextTarget == nextOut)//"full"
	return NULL;
      T* ret = &data[target];
      constexpr if(std::is_copy_constructable<T>::value) {
	*ret = *src;
      } else {
	memcpy(reinterpret_cast<void*>(ret), reinterpret_cast<void*>(src), sizeof(T));
      }
      return ret;
    }

    bool pop(T* out) {
      size_t target = nextOut,
	nextTarget = wrap(target + 1);
      //we always have 1 empty slot, or else we couldn't tell full from empty because lastIn == nextOut
      if(nextTarget == nextIn)//empty
	  return false;
      constexpr if(std::is_copy_constructable<T>::value) {
	*out = data[target];
      } else {
	memcpy(reinterpret_cast<void*>(out), reinterpret_cast<void*>(&data[target]), sizeof(T));
      }
      nextOut = nextTarget;
    }

    inline size_t count() {
      size_t tail = nextOut, head = nextIn;
      return tail < head ? SIZE - head + tail : tail - head;
    }

    inline size_t freeSpace() {
      return SIZE - count() - 1;//there is always an empty slot
    }

    size_t bulkPush(T* in, size_t count, T** handleOut) {
      size_t target = nextIn, nextTarget, targetCount;
      size_t tail = nextOut;
      targetCount = glm::min(count, tail > target ? SIZE - tail + target - 1 : target - tail);
      if(!targetCount)
	return 0;//"full"
      nextTarget = wrap(target + targetCount);
      nextIn = nextTarget;
      for(int i = 0, j = target;i < targetCount;i++) {
	constexpr if(std::is_copy_constructable<T>::value) {
	  data[j] = in[i];
	} else {
	  memcpy(reinterpret_cast<void*>(&data[j]), reinterpret_cast<void*>(&in[i]), sizeof(T));
	}
	if(handleOut)
	  handleOut[i] = &data[j];
	j = wrap(j + 1);
      }
      return ret;
    }

    //bulk pop with predicate callback that performs binary search (for "pop only deltas from previous frames")
    size_t bulkPop(T* out, size_t count, Callback_t<bool, T*>* condition) {
      size_t target = nextOut, nextTarget, targetCount;
      if(condition && !condition=->call(&data[target]))
	return 0;
      size_t head = nextCommit;
      targetCount = glm::min(count, head < target ? SIZE - target + head : head - target);
      if(!targetCount)
	return 0;
      //condition binary search
      if(condition && !condition->call(&data[wrap(target + targetCount)])) {
	size bottom = 0, middle = targetCount / 2;
	while(targetCount - bottom > 1) {
	  if(condition->call(&data[wrap(target + middle)]))
	    bottom = middle;
	  else
	    targetCount = middle;
	  middle = (bottom + targetCount) / 2;
	}
	while(!condition->call(&data[wrap(target + targetCount)])) targetCount--;//at most 2 iterations
      }
      nextTarget = wrap(target + targetCount);
      //we always have 1 empty slot, or else we couldn't tell full from empty because lastIn == nextOut
      constexpr if(std::is_copy_constructable<T>::value) {
	for(int i = 0, j = target;i < targetCount;i++) {
	  out[i] = data[j];
	  j = wrap(j + 1);
	}
      } else {
	if(newTarget > target) {
	  memcpy(reinterpret_cast<void*>(out), reinterpret_cast<void*>(&data[target]), sizeof(T) * targetCount);
	} else {
	  size_t split = SIZE - target - 1;
	  memcpy(reinterpret_cast<void*>(out), reinterpret_cast<void*>(&data[target]), sizeof(T) * split);
	  memcpy(reinterpret_cast<void*>(&out[split]), reinterpret_cast<void*>(data), sizeof(T) * (targetCount - split));
	}
      }
      return targetCount;
    }

  };

}