#include "Thread.hpp"
#include "StdExtensions.hpp"
#include "Math.hpp"

namespace WITE::Collections {

  //thread safe and fast, well ordered fifo queue of fixed size.
  template<typename T> class AtomicRollingQueue {
  public:
    typedefCB(popCondition_t, bool, const T*);
  private:
    std::unique_ptr<T[]> data;
    size_t size;
    typedef struct {
      std::atomic_size_t claimed, committed;
    } transactionalIndex;
    transactionalIndex nextIn, nextOut;

    size_t wrap(size_t i) {
      return i >= size ? i >= size * 2 ? i % size : i - size : i;
    };

    bool boundedIncrement(transactionalIndex& target, transactionalIndex& bounds, size_t& out, bool allowBoundHit, popCondition_t pc = NULL) {
      out = target.claimed.load(std::memory_order_acquire);
      size_t newValue;
      do {
	newValue = wrap(out+1);
	size_t boundValue = bounds.committed.load(std::memory_order_consume);
	if(newValue == (allowBoundHit ? wrap(boundValue + 1) : boundValue))
	  return false;
	if(pc && !pc(&data[out]))
	  return false;
      } while(!target.claimed.compare_exchange_weak(out, newValue, std::memory_order_release, std::memory_order_acq_rel));
      return true;
    };

    void committalIncrement(transactionalIndex& idx, size_t target) {
      size_t temp = target, nextTarget = wrap(target+1);
      while(!idx.committed.compare_exchange_weak(temp, nextTarget, std::memory_order_release, std::memory_order_relaxed))
	temp = target;
    };

  public:

    AtomicRollingQueue(size_t size) : data(std::make_unique<T[]>(size+1)), size(size+1) {}

    bool contains(T& ref) {
      for(size_t i =nextOut.claimed.load(std::memory_order_relaxed);
	  i != nextIn.claimed.load(std::memory_order_relaxed);
	  i = wrap(i + 1))
	if(data[i] == ref)
	  return true;
      return false;
    }

    //only blocks if after an unfinished call to the same method, blocks until that call completes, thus becomming strongly-after.
    T* push(T* src) {
      size_t target;
      if(!boundedIncrement(nextIn, nextOut, target, false))
	return NULL;
      T* ret = &data[target];
      if constexpr(std::is_copy_constructible<T>::value) {
	*ret = *src;
      } else {
	memcpy(ret, src, sizeof(T));
      }
      committalIncrement(nextIn, target);
      return ret;
    }

    bool pop(T* out) {//only for cases when other threads do not care about the popped data (not for log transfers)
      size_t target;
      if(!boundedIncrement(nextOut, nextIn, target, true))
	return false;
      if constexpr (std::is_copy_constructible<T>::value) {
	*out = data[target];
      } else {
	memcpy(out, &data[target], sizeof(T));
      }
      committalIncrement(nextOut, target);
      return true;
    };

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

    T pop() {
      std::enable_if_t<std::is_pointer_v<T>, T> ret;
      if(!pop(&ret))
	return (T)0;
      return ret;
    }

    T& dirtyPeek() {
      return data[nextOut.claimed.load(std::memory_order_relaxed)];
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
      size_t start = nextOut.claimed.load(std::memory_order_consume),
	end = nextIn.claimed.load(std::memory_order_consume);
      return start <= end ? end - start : size - (start - end);
    }

    inline size_t freeSpace() const {
      return size - count();
    }

    class PoppingTransaction {//TODO bulk pop with transaction
    private:
      AtomicRollingQueue<T>* super;
      size_t target;
      bool hasValue;
    public:
      PoppingTransaction(AtomicRollingQueue<T>* s, popCondition_t pc) : super(s) {
	hasValue = super->boundedIncrement(super->nextOut, super->nextIn, target, true, pc);
      };
      PoppingTransaction(PoppingTransaction& other) : super(other.super),
						      target(other.target),
						      hasValue(other.hasValue) {
	other.hasValue = false;
      };
      ~PoppingTransaction() {
	if(!hasValue) return;
	super->committalIncrement(super->nextOut, target);
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
      size_t ocl = arq.nextOut.claimed, oco = arq.nextOut.committed, icl = arq.nextIn.claimed, ico = arq.nextIn.committed;
      str << "{" << std::endl <<
	"nextOut.claimed: (" << ocl << ") " << arq.data[ocl] << std::endl <<
	", nextOut.committed: (" << oco << ") " << arq.data[oco] << std::endl <<
	", nextIn.claimed: (" << icl << ") " << arq.data[icl] << std::endl <<
	", nextIn.committed: (" << ico << ") " << arq.data[ico] << std::endl <<
	", count: " << arq.count() << std::endl <<
	", free space: " << arq.freeSpace() << std::endl <<
	" }" << std::endl;
      return str;
    };

  };

}
