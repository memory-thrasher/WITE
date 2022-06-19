#include "Thread.hpp"
#include "StdExtensions.hpp"
#include "Math.hpp"

namespace WITE::Collections {

  //thread safe and fast, well ordered fifo queue of fixed size.
  template<typename T> class AtomicRollingQueue {
  private:
    std::unique_ptr<T[]> data;
    size_t size;
    std::atomic_size_t nextCommit,//the index of a datum that has not finished being written. Data below this are readable.
      nextIn,//the index of an empty datum that is next to be allocated
      nextOut;//the index of the next datum to be popped
    size_t wrap(size_t i) {
      return i >= size ? i >= size * 2 ? i % size : i - size : i;
    };
  public:

    AtomicRollingQueue(size_t size) : data(std::make_unique<T[]>(size)), size(size) {}

    //only blocks if after an unfinished call to the same method, blocks until that call completes, thus becomming strongly-after.
    T* push(T* src) {
      size_t target = nextIn.load(std::memory_order_acquire), nextTarget;
      do {
	nextTarget = wrap(target + 1);
	//we always have 1 empty slot, or else we couldn't tell full from empty because lastIn == nextOut
	if(nextTarget == nextOut.load(std::memory_order_consume))//"full"
	  return NULL;
      } while(!nextIn.compare_exchange_strong(target, nextTarget, std::memory_order_release, std::memory_order_acquire));
      T* ret = &data[target];
      if constexpr(std::is_copy_constructible<T>::value) {
	*ret = *src;
      } else {
	memcpy(ret, src, sizeof(T));
      }
      size_t temp = target;
      while(!nextCommit.compare_exchange_strong(temp, nextTarget, std::memory_order_release, std::memory_order_relaxed))
	temp = target;
      return ret;
    }

    T* push(T src) {//only recommended for trivial or pimitive T
      return push(&src);
    }

    T* push_blocking(T* src) {
      T* ret = push(src);
      while(ret == NULL) {
	Platform::Thread::sleep();
	ret = push(src);
      }
      return ret;
    }

    bool pop(T* out) {
      //concurrent pop is supported but highly unlikely so not optimized
      size_t target = nextOut.load(std::memory_order_acquire), nextTarget;
      do {
	nextTarget = wrap(target + 1);
	//we always have 1 empty slot, or else we couldn't tell full from empty because lastIn == nextOut
	if(nextTarget == nextCommit.load(std::memory_order_consume))//empty
	  return false;
	if constexpr(std::is_copy_constructible<T>::value) {
	  *out = data[target];
	} else {
	  memcpy(out, &data[target], sizeof(T));
	}
      } while(!nextOut.compare_exchange_strong(target, nextTarget, std::memory_order_release, std::memory_order_acquire));
      return true;
    };

    template<typename R = std::enable_if_t<std::is_pointer_v<T>, T>> R pop() {
      T ret;
      if(!pop(&ret))
	return (T)0;
      return ret;
    }

    void pop_blocking(T* out) {
      while(!pop(out)) Platform::Thread::sleep();
    }

    T& pop_blocking() {
      T ret;
      pop_blocking(&ret);
      return ret;
    }

    size_t count() {
      size_t tail = nextOut.load(std::memory_order_consume), head = nextIn.load(std::memory_order_consume);
      return tail < head ? size - head + tail : tail - head;
    }

    size_t bulkPush(T* in, size_t count, T** handleOut) {
      size_t target = nextIn.load(std::memory_order_acquire), nextTarget, targetCount;
      do {
	size_t tail = nextOut.load(std::memory_order_consume);
	targetCount = Util::min(count, tail > target ? size - tail + target - 1 : target - tail);
	if(!targetCount)
	  return 0;//"full"
	nextTarget = wrap(target + targetCount);
      } while(!nextIn.compare_exchange_strong(target, nextTarget, std::memory_order_release, std::memory_order_acquire));
      for(int i = 0, j = target;i < targetCount;i++) {
	if constexpr(std::is_copy_constructible<T>::value) {
	  data[j] = in[i];
	} else {
	  memcpy(&data[j], &in[i], sizeof(T));
	}
	if(handleOut)
	  handleOut[i] = &data[j];
	j = wrap(j + 1);
      }
      size_t temp = target;
      while(!nextCommit.compare_exchange_strong(temp, nextTarget, std::memory_order_release, std::memory_order_relaxed))
	temp = target;
      return targetCount;
    }

    //bulk pop with predicate callback that performs binary search (for "pop only deltas from previous frames")
    size_t bulkPop(T* out, size_t count, Util::Callback_t<bool, T*>* condition) {
      //concurrent pop is supported but highly unlikely so not optimized
      size_t target = nextOut.load(std::memory_order_acquire), nextTarget, targetCount;
      do {
	if(!condition->call(&data[target]))
	  return 0;
	size_t head = nextCommit.load(std::memory_order_acquire);
	targetCount = Util::min(count, head < target ? size - target + head : head - target);
	if(!targetCount)
	  return 0;
	//condition binary search
	if(!condition->call(&data[wrap(target + targetCount)])) {
	  size_t bottom = 0, middle = targetCount / 2;
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
	if constexpr(std::is_copy_constructible<T>::value) {
	  for(int i = 0, j = target;i < targetCount;i++) {
	    out[i] = data[j];
	    j = wrap(j + 1);
	  }
	} else {
	  if(nextTarget > target) {
	    memcpy(out, &data[target], sizeof(T) * targetCount);
	  } else {
	    size_t split = size - target - 1;
	    memcpy(out, &data[target], sizeof(T) * split);
	    memcpy(&out[split], data, sizeof(T) * (targetCount - split));
	  }
	}
      } while(!nextOut.compare_exchange_strong(target, nextTarget, std::memory_order_release, std::memory_order_acquire));
      return targetCount;
    }

  };

}
