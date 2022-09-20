#include "Thread.hpp"
#include "StdExtensions.hpp"
#include "Math.hpp"

namespace WITE::Collections {

  //thread safe and fast, well ordered fifo queue of fixed size.
  template<typename T> class AtomicRollingQueue {
  private:
    std::unique_ptr<T[]> data;
    size_t size;
    //TODO make this a singular struct too
    std::atomic_size_t nextCommit,//the index of a datum that has not finished being written. Data below this are readable.
      nextIn,//the index of an empty datum that is next to be allocated
      nextPopRead,//TODO name this,//the index of oldest readable entry, in the process of being popped
      nextPopDrop;//the index of the next datum to be popped
    size_t wrap(size_t i) {
      return i >= size ? i >= size * 2 ? i % size : i - size : i;
    };
  public:

    AtomicRollingQueue(size_t size) : data(std::make_unique<T[]>(size)), size(size),
				      nextCommit(0), nextIn(0),
				      nextPopRead(0), nextPopDrop(0) {}

    bool contains(T& ref) {
      for(size_t i = nextPopDrop;i != nextIn;i = wrap(i + 1))
	if(data[i] == ref)
	  return true;
      return false;
    }

    //only blocks if after an unfinished call to the same method, blocks until that call completes, thus becomming strongly-after.
    T* push(T* src) {
      size_t target = nextIn.load(std::memory_order_acquire), nextTarget;
      do {
	nextTarget = wrap(target + 1);
	//we always have 1 empty slot, or else we couldn't tell full from empty because lastIn == nextPopRead
	if(nextTarget == nextPopDrop.load(std::memory_order_consume))//"full"
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

    bool pop(T* out) {//only for cases when other threads do not care about the popped data (not for log transfers)
      size_t target = nextPopRead.load(std::memory_order_acquire), nextTarget;
      do {
	nextTarget = wrap(target + 1);
	//we always have 1 empty slot, or else we couldn't tell full from empty because lastIn == nextOut
	if(nextTarget == nextCommit.load(std::memory_order_consume))//empty
	  return false;
      } while(!nextPopRead.compare_exchange_strong(target, nextTarget, std::memory_order_release, std::memory_order_acq_rel));
      if constexpr (std::is_copy_constructible<T>::value) {
	*out = data[target];
      } else {
	memcpy(out, &data[target], sizeof(T));
      }
      while(nextPopDrop.load(std::memory_order_acquire) != target)
	Platform::Thread::sleep();
      nextPopDrop.store(nextTarget, std::memory_order_release);
      return true;
    };

    T pop() {
      std::enable_if_t<std::is_pointer_v<T>, T> ret;
      if(!pop(&ret))
	return (T)0;
      return ret;
    }

    T& dirtyPeek() {
      return data[nextPopDrop];
    }

    void pop_blocking(T* out) {
      while(!pop(out)) Platform::Thread::sleep();
    }

    T& pop_blocking() {
      T ret;
      pop_blocking(&ret);
      return ret;
    }

    size_t count() const {
      size_t tail = nextIn.load(std::memory_order_consume), head = nextPopRead.load(std::memory_order_consume);
      return head <= tail ? tail - head : size - head + tail;
    }

    size_t freeSpace() const {
      size_t tail = nextIn.load(std::memory_order_consume), head = nextPopDrop.load(std::memory_order_consume);
      size_t count = head <= tail ? tail - head : size - head + tail;
      return size - count - 1;//there is always an empty slot
    }

    size_t freeSpacePending() const {
      return size - count() - 1;//there is always an empty slot
    }

    typedefCB(popCondition_t, bool, T*);

    class PoppingTransaction {//TODO bulk pop with transaction
    private:
      AtomicRollingQueue<T>* super;
      size_t target, nextTarget;
      bool hasValue;
    public:
      PoppingTransaction(AtomicRollingQueue<T>* s, popCondition_t pc) : super(s) {
	target = super->nextPopRead.load(std::memory_order_acquire);
	hasValue = true;
	do {
	  nextTarget = super->wrap(target + 1);
	  //we always have 1 empty slot, or else we couldn't tell full from empty because lastIn == nextOut
	  if(nextTarget == super->nextCommit.load(std::memory_order_consume) ||
	     (pc && !pc(&super->data[target]))) {//empty
	    hasValue = false;
	    return;
	  }
	} while(!super->nextPopRead.compare_exchange_strong(target, nextTarget,
							    std::memory_order_release, std::memory_order_acq_rel));
      };
      PoppingTransaction(PoppingTransaction& other) : super(other.super),
						      target(other.target),
						      nextTarget(other.nextTarget),
						      hasValue(other.hasValue) {
	other.hasValue = false;
      };
      ~PoppingTransaction() {
	if(!hasValue) return;
	while(super->nextPopDrop.load(std::memory_order_acquire) != target);
	  //Platform::Thread::sleep();
	super->nextPopDrop.store(nextTarget, std::memory_order_release);
      };
      operator bool() const {
	return hasValue;
      };
      const T* operator->() const {
	ASSERT_TRAP(hasValue, "Attempted to dereference null popping transaction");
	return &super->data[target];
      };
      const T* operator*() const {
	ASSERT_TRAP(hasValue, "Attempted to dereference null popping transaction");
	return &super->data[target];
      };
    };

    inline PoppingTransaction transactionalPop(popCondition_t pc) {
      return PoppingTransaction(this, pc);
    };

    friend std::ostream& operator<<(std::ostream& str, const AtomicRollingQueue<T>& arq) {
      //TODO dynamically sense if T can be <<‘d to stdout
      // str << "{ nextCommit: " << arq.nextCommit.load() << ", nextIn: " << arq.nextIn.load() << ", nextPopRead: " <<
      // 	arq.nextPopRead.load() << ", nextPopDrop: " << arq.nextPopDrop.load() << " }";
      auto nc = arq.nextCommit.load(), ni = arq.nextIn.load(), npr = arq.nextPopRead.load(), npd = arq.nextPopDrop.load();
      str << "{" << std::endl <<
	"nextCommit: (" << nc << ") " << arq.data[nc] << std::endl <<
	", nextIn: (" << ni << ") " << arq.data[ni] << std::endl <<
	", nextPopRead: (" << npr << ") " << arq.data[npr] << std::endl <<
	", nextPopDrop: (" << npd << ") " << arq.data[npd] << std::endl <<
	", count: " << arq.count() << std::endl <<
	", free space: " << arq.freeSpace() << std::endl <<
	" }" << std::endl;
      return str;
    };

  };

}
