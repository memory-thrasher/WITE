#pragma once

#include "constants.h"

namespace WITE {
  /*
    Manages a single data buffer, tracking a head and a tail, allowing data to be pushed to the tail and popped from the head without
    memmov'ing the rest of the data. Each operation constitutes one or two memcpys.
    Not completely thread safe, though operations are ordered to gaurantee well defined behavior
    so long as the following conditions are met:
    >at most one thread pushes
    >at most one thread pops
    >realloc is disabled
    >no threads are calling relocate <<< TODO may need this
    Any number of threads can read concurrently, though may experience dirty reads on the head
    if another thread is concurrently performing a push or relocate.
  */
  template<class HeadType, typename SIZE_TYPE = size_t, SIZE_TYPE DATA_SIZE = 0, SIZE_TYPE ENTRY_SIZE = sizeof(HeadType),
    SIZE_TYPE sizeFieldOffset = std::numeric_limits<size_t>::max(), bool ALLOW_REALLOC = true>
  class export_def RollingBuffer {
  private:
  typedef struct {
    SIZE_TYPE size;
    HeadType head;
  } HeadWithSizeType;
  public:
  template<SIZE_TYPE cloneSize = DATA_SIZE, SIZE_TYPE cloneCount = 1 + cloneSize / ENTRY_SIZE> class RBIterator;
  constexpr static bool INLINEDATA = bool(DATA_SIZE);
  constexpr static bool VARIABLE_RECORD_SIZE = !bool(ENTRY_SIZE);
  constexpr static bool ADD_SIZE = ((std::numeric_limits<size_t>::max() == sizeFieldOffset) && VARIABLE_RECORD_SIZE);
  constexpr static bool RESIZABLE = (!INLINEDATA && ALLOW_REALLOC);
  constexpr static bool MOVABLE = (!INLINEDATA);
  //size in bytes
  template<bool ID = INLINEDATA, typename std::enable_if_t<!ID, size_t> = 0> RollingBuffer(SIZE_TYPE size, uint8_t* data = NULL);
  RollingBuffer();
  ~RollingBuffer();
  template<bool AS = ADD_SIZE> typename std::enable_if_t<AS, uint16_t> push(HeadType* newData, SIZE_TYPE size, size_t* handleOut = NULL);
  template<bool AS = ADD_SIZE> typename std::enable_if_t<AS, uint16_t> pushBlocking(HeadType* newData, SIZE_TYPE size, size_t* handleOut = NULL);
  template<bool AS = ADD_SIZE> typename std::enable_if_t<!AS, uint16_t> push(HeadType* newData, size_t* handleOut = NULL);
  template<bool AS = ADD_SIZE> typename std::enable_if_t<!AS, uint16_t> pushBlocking(HeadType* newData, size_t* handleOut = NULL);
  /*returns one of the constants above. maxsize in bytes returned. max in count entries.
    First entry starts at data[0], second at data[starts[0]], data[starts[1]] ... until starts[n] = 0*/
  uint16_t pop(uint8_t* data, size_t* starts, size_t maxsize, size_t max);
  //Like pop but does not alter the stack, but does take an optional position tracker
  uint16_t peek(uint8_t* data, size_t* starts, size_t maxsize, size_t max, SIZE_TYPE* pos);
  uint16_t drop();//remove the next element without reading it
  SIZE_TYPE used();
  SIZE_TYPE count();
  SIZE_TYPE freeSpace();
  void readFromHandle(uint8_t* out, SIZE_TYPE in, SIZE_TYPE size);//careful; only checked for wrap
  void writeFromHandle(SIZE_TYPE out, uint8_t* in, SIZE_TYPE size);//careful; only checked for wrap
  //NOT thread safe; ensure no threads are doing anything while this is executing
  template<bool M = MOVABLE> typename std::enable_if_t<M, uint16_t> relocate(uint8_t* newhome, SIZE_TYPE newsize);
  template<bool R = RESIZABLE> typename std::enable_if_t<R, uint16_t> resize(SIZE_TYPE size);
  //grow if necessary, plus pad bytes
  template<bool R = RESIZABLE> typename std::enable_if_t<R, uint16_t> swallow(uint8_t* newData, size_t dataSize, uint64_t* handleOut = NULL, size_t pad = 0);
  inline SIZE_TYPE sizeofnext(SIZE_TYPE);
  inline SIZE_TYPE sizeofnext() { return sizeofnext(head.load(/*std::memory_order_consume*/)); };
  template<SIZE_TYPE cloneSize = DATA_SIZE, SIZE_TYPE cloneCount = 1 + cloneSize / ENTRY_SIZE>
  inline RBIterator<cloneSize, cloneCount> iter();
  private:
  RollingBuffer(const RollingBuffer&) = delete;
  void readRaw(uint8_t* out, SIZE_TYPE in, SIZE_TYPE size);//careful; only checked for wrap
  void writeRaw(SIZE_TYPE out, uint8_t* in, SIZE_TYPE size);//careful; only checked for wrap
  inline void BOUND_INC_HEAD(SIZE_TYPE amnt);
  inline void BOUND_INC_TAIL(SIZE_TYPE amnt);
  std::atomic<SIZE_TYPE> head, tail, handleOffset;//TODO non-atomic option
  SIZE_TYPE size = 0, entries = 0;
  uint8_t * buf;
  bool outer = false;//when head = tail; true means buffer is full, false means empty, otherwise this should be false
  uint8_t inline_data[DATA_SIZE ? DATA_SIZE : 1];
  inline bool wrapped() { return outer || head.load(/*std::memory_order_consume*/) > tail.load(/*std::memory_order_consume*/); };
  public:
  template<SIZE_TYPE cloneSize, SIZE_TYPE cloneCount>
  class RBIterator {//designed to be placed on the thread stack, used as a read buffer for batch reading
  public:
    typedef RollingBuffer<HeadType, SIZE_TYPE, DATA_SIZE, ENTRY_SIZE, sizeFieldOffset, ALLOW_REALLOC> master_t;
  private:
    master_t* master;
    SIZE_TYPE readHead, i;
    uint8_t data[cloneSize];
    size_t starts[cloneCount];
  public:
    RBIterator(master_t* rb) : master(rb), readHead(rb->head.load(/*std::memory_order_consume*/)), i(std::numeric_limits<SIZE_TYPE>::lowest()) {}
    SIZE_TYPE getHead() { return i ? starts[i - 1] : 0; }
    HeadType* next() {//returns null when out
      HeadType* ret;
      if (i == std::numeric_limits<SIZE_TYPE>::lowest() || !starts[i-1]) {
	if (master->peek(data, starts, cloneSize, cloneCount, &readHead) == RB_BUFFER_UNDERFLOW) return NULL;
	i = 0;
      }
      ret = static_cast<HeadType*>(static_cast<void*>(data + (i ? starts[i-1] : 0)));
      i++;
      return ret;
    }
  };
  };

//#define BOUND_INC_HEAD(amnt) {SIZE_TYPE h = head.load(/*std::memory_order_acquire*/) + amnt; if(h >= size) h -= size; outer = false; \
//    handleOffset.fetch_add(-amnt/*, std::memory_order_acq_rel*/); head.store(h/*, std::memory_order_release*/); }
//#define BOUND_INC_TAIL(amnt) {SIZE_TYPE t = tail.load(/*std::memory_order_acquire*/) + amnt; if(t >= size) t -= size; \
//    outer = head.load(/*std::memory_order_consume*/) == t; tail.store(t/*, std::memory_order_release*/); }

#define RB_PROTO(...) template<class HeadType, typename SIZE_TYPE, SIZE_TYPE DATA_SIZE, SIZE_TYPE ENTRY_SIZE, \
			       SIZE_TYPE sizeFieldOffset, bool ALLOW_REALLOC> \
  __VA_ARGS__ RollingBuffer<HeadType, SIZE_TYPE, DATA_SIZE, ENTRY_SIZE, sizeFieldOffset, ALLOW_REALLOC>
#define RB_FQN RollingBuffer<HeadType, SIZE_TYPE, DATA_SIZE, ENTRY_SIZE, sizeFieldOffset, ALLOW_REALLOC>

  RB_PROTO(template<bool ID, typename std::enable_if_t<!ID, size_t>>)::RollingBuffer(SIZE_TYPE size, uint8_t* data) :
    head(0), tail(0), handleOffset(0), size(size), buf(data ? data : malloc(size)) {};

  RB_PROTO()::RollingBuffer() : head(0), tail(0), handleOffset(0), size(DATA_SIZE), buf(DATA_SIZE ? inline_data : NULL) {}

  RB_PROTO()::~RollingBuffer() {
    if (ALLOW_REALLOC) std::free((void*)(buf));
  }

  RB_PROTO(void)::BOUND_INC_HEAD(SIZE_TYPE amnt) {
    SIZE_TYPE computed, original;
    do {
      original = head.load();
      computed = original + amnt;
      if(computed >= size) computed -= size;
    } while(!head.compare_exchange_strong(original, computed));
    outer = false;
    handleOffset.fetch_add(-amnt);
  }

  RB_PROTO(void)::BOUND_INC_TAIL(SIZE_TYPE amnt) {
    SIZE_TYPE computed, original;
    do {
      original = tail.load();
      computed = original + amnt;
      if(computed >= size) computed -= size;
    } while(!tail.compare_exchange_strong(original, computed));
    outer = head.load() == computed;
  }

  RB_PROTO(template<SIZE_TYPE cloneSize, SIZE_TYPE cloneCount> RB_FQN::RBIterator<cloneSize, cloneCount>)::iter() {
    RBIterator<cloneSize, cloneCount> ret(this);
    return ret;
  }

  RB_PROTO(SIZE_TYPE)::used() {
    size_t ret;
    SIZE_TYPE head = this->head.load(/*std::memory_order_consume*/),
      tail = this->tail.load(/*std::memory_order_consume*/);
    ret = head == tail ? (outer ? size : 0) : head < tail ? tail - head : tail + size - head;
    return ret;
  }

  RB_PROTO(SIZE_TYPE)::count() {
    return entries;
  }

  RB_PROTO(SIZE_TYPE)::freeSpace() {
    return size - used();
  }

  RB_PROTO(void)::readFromHandle(uint8_t* out, SIZE_TYPE in, SIZE_TYPE size) {
    readRaw(out, in + handleOffset.load(/*std::memory_order_consume*/) + head.load(/*std::memory_order_consume*/), size);
  }

  RB_PROTO(void)::writeFromHandle(SIZE_TYPE out, uint8_t* in, SIZE_TYPE size) {
    writeRaw(out + handleOffset.load(/*std::memory_order_consume*/) + head.load(/*std::memory_order_consume*/), in, size);
  }

  RB_PROTO(void)::readRaw(uint8_t* out, SIZE_TYPE offset, SIZE_TYPE readSize) {
    if (offset >= size) offset %= size;
    if (offset + readSize >= size) {//:(
      memcpy(out, &buf[offset], size - offset);
      memcpy(&out[size - offset], buf, readSize + offset - size);
    } else {
      memcpy(out, &buf[offset], readSize);
    }
  }

  RB_PROTO(void)::writeRaw(SIZE_TYPE offset, uint8_t* in, SIZE_TYPE writeSize) {
    if (offset >= size) offset %= size;
    if (offset + writeSize >= size) {//:(
      memcpy(&buf[offset], in, size - offset);
      memcpy(buf, &in[size - offset], writeSize + offset - size);
    } else {
      memcpy(&buf[offset], in, writeSize);
    }
  }

  RB_PROTO(SIZE_TYPE)::sizeofnext(SIZE_TYPE h) {
    SIZE_TYPE offset;
    if (ENTRY_SIZE) return ENTRY_SIZE;
    offset = ADD_SIZE ? offsetof(HeadWithSizeType, size) : sizeFieldOffset;
    union {
      SIZE_TYPE ret;
      uint8_t retRaw[1];
    };
    readRaw(retRaw, offset + h, sizeof(SIZE_TYPE));
    return ret;
  }

  RB_PROTO(uint16_t)::drop() {
    SIZE_TYPE sizeToDrop;
    if (!used()) return RB_BUFFER_UNDERFLOW;
    sizeToDrop = sizeofnext() + (ADD_SIZE ? sizeof(HeadWithSizeType) : sizeof(HeadType));
    entries--;
    BOUND_INC_HEAD(sizeToDrop);
    return RB_SUCCESS;
  }

  RB_PROTO(template<bool AS> typename std::enable_if_t<AS, uint16_t>)::push(HeadType* newData, SIZE_TYPE dataSize, size_t* handleOut) {
    if (dataSize + sizeof(HeadWithSizeType) > freeSpace()) return RB_BUFFER_OVERFLOW;
    if (handleOut) *handleOut = used() - handleOffset.load(/*std::memory_order_consume*/);
    writeRaw(tail, dataSize, sizeof(SIZE_TYPE));
    writeRaw(tail + sizeof(SIZE_TYPE), newData, dataSize + sizeof(HeadType));
    BOUND_INC_TAIL(dataSize + sizeof(HeadWithSizeType));
    entries++;
    return RB_SUCCESS;
  }

  RB_PROTO(template<bool AS> typename std::enable_if_t<AS, uint16_t>)::pushBlocking(HeadType* newData, SIZE_TYPE dataSize, size_t* handleOut) {
    while(dataSize + sizeof(HeadWithSizeType) > freeSpace());//TODO yield?
    if(handleOut) *handleOut = used() - handleOffset.load(/*std::memory_order_consume*/);
    writeRaw(tail, dataSize, sizeof(SIZE_TYPE));
    writeRaw(tail + sizeof(SIZE_TYPE), newData, dataSize + sizeof(HeadType));
    BOUND_INC_TAIL(dataSize + sizeof(HeadWithSizeType));
    entries++;
    return RB_SUCCESS;
  }

  RB_PROTO(template<bool AS> typename std::enable_if_t<!AS, uint16_t>)::push(HeadType* newData, size_t* handleOut) {
    SIZE_TYPE dataSize;
    memcpy(&dataSize, (uint8_t*)newData + sizeFieldOffset, sizeof(SIZE_TYPE));
    dataSize += sizeof(HeadType);
    if (dataSize + sizeof(HeadType) > freeSpace()) return RB_BUFFER_OVERFLOW;
    if(handleOut) *handleOut = used() - handleOffset.load(/*std::memory_order_consume*/);
    writeRaw(tail, (uint8_t*)newData, dataSize);
    BOUND_INC_TAIL(dataSize);
    entries++;
    return RB_SUCCESS;
  }

  RB_PROTO(template<bool AS> typename std::enable_if_t<!AS, uint16_t>)::pushBlocking(HeadType* newData, size_t* handleOut) {
    SIZE_TYPE dataSize;
    memcpy(&dataSize, (uint8_t*)newData + sizeFieldOffset, sizeof(SIZE_TYPE));
    dataSize += sizeof(HeadType);
    while(dataSize + sizeof(HeadType) > freeSpace());//TODO yield?
    if(handleOut) *handleOut = used() - handleOffset.load(/*std::memory_order_consume*/);
    writeRaw(tail, (uint8_t*)newData, dataSize);
    BOUND_INC_TAIL(dataSize);
    entries++;
    return RB_SUCCESS;
  }

  RB_PROTO(uint16_t)::pop(uint8_t* data, size_t* starts, size_t maxsize, size_t max) {
    SIZE_TYPE subsize, totalWrote = 0;
    uint16_t ret;
    if (!entries) return RB_BUFFER_UNDERFLOW;
    ret = RB_SUCCESS;
    if (entries >= max) ret = RB_READ_INCOMPLETE;
    else max = entries;
    while (max && (subsize = sizeofnext() + sizeof(HeadType)) && subsize <= maxsize) {
      readRaw(data, head.load(/*std::memory_order_consume*/), subsize);
      entries--;
      BOUND_INC_HEAD(subsize);
      max--;
      maxsize -= subsize;
      data += subsize;
      *starts = totalWrote += subsize;
      starts++;
    }
    if (max) ret = RB_READ_INCOMPLETE;
    starts[-1] = 0;
    return ret;
  }

  RB_PROTO(uint16_t)::peek(uint8_t* data, size_t* starts, size_t maxsize, size_t max, SIZE_TYPE* pos) {
    SIZE_TYPE subsize, totalWrote = 0, subpos;
    uint16_t ret;
    if (!entries) return RB_BUFFER_UNDERFLOW;
    if (!pos) {
      subpos = head.load(/*std::memory_order_consume*/);
      pos = &subpos;
    }
    ret = RB_SUCCESS;
    if (entries >= max) ret = RB_READ_INCOMPLETE;
    else max = entries;
    while (max && (subsize = sizeofnext(*pos) + sizeof(HeadType)) && subsize <= maxsize) {
      readRaw(data, *pos, subsize);
      *pos += subsize;//TODO if ADD_SIZE, add size here?
      if (*pos >= size) *pos -= size;
      max--;
      maxsize -= subsize;
      data += subsize;
      *starts = totalWrote += subsize;
      starts++;
    }
    if (max) ret = RB_READ_INCOMPLETE;
    starts[-1] = 0;
    return ret;
  }

  //NOT thread safe; ensure no threads are doing anything while this is executing
  RB_PROTO(template<bool M> typename std::enable_if_t<M, uint16_t>)::relocate(uint8_t* newStart, SIZE_TYPE newSize) {
    uint8_t* newEnd, *oldStart, *oldEnd;
    size_t used = this->used(), leadingEmpty, trailingEmpty, mov;
    int64_t oldHead, oldTail, startOffset, movRemaining;
    bool wasWrapped;
    if (newSize < used) return RB_BUFFER_OVERFLOW;
    SIZE_TYPE head = this->head.load(/*std::memory_order_seq_cst*/), tail = this->tail.load(/*std::memory_order_seq_cst*/);
    newEnd = newStart + newSize;
    oldStart = (uint8_t*)buf;
    oldEnd = oldStart + size;
    startOffset = static_cast<int64_t>(oldStart - newStart);
    oldHead = head + startOffset;//in new space
    oldTail = tail + startOffset;
    if (!used) {//nothing to move; ideal
      outer = false;
      head = tail = 0;
    } else if(std::less<void*>()(newStart, oldEnd) && std::less<void*>()(oldStart, newEnd)) {//overlap, common
      wasWrapped = outer || tail < head;
      leadingEmpty = std::less<void*>()(newStart, oldStart) ? startOffset : 0;
      trailingEmpty = std::less<void*>()(newEnd, oldEnd) ? 0 : static_cast<size_t>(newEnd - oldEnd);
      if (wasWrapped) {//one end must move, pick smaller
	if (size - head > tail) {//move tail
	  if (trailingEmpty) {//move tail to fill void
	    movRemaining = tail;
	    mov = min(movRemaining, trailingEmpty);
	    memcpy(oldEnd, oldStart, mov);
	    movRemaining -= mov;
	    if (movRemaining > 0) {
	      memmove(newStart, oldStart + mov, movRemaining);
	      tail = movRemaining;
	    } else
	      tail = newSize + movRemaining;//movRemaining <= 0
	    head = oldHead;
	  } else {//case: newend <= oldend: trailingEmpty = 0, must relocate segment of head at end to beginning
	    mov = static_cast<size_t>(oldEnd - newEnd);
	    if (mov != startOffset) memmove(newStart + mov, oldStart, tail);
	    memcpy(newStart, newEnd, mov);
	    tail = mov + tail;
	    head = oldHead;
	  }
	} else {//move head
	  if (leadingEmpty) {//move head back to fill void
	    movRemaining = size - head;
	    mov = min<int64_t>(movRemaining, leadingEmpty);
	    if (mov) memcpy(newStart - mov + leadingEmpty, oldEnd - mov, mov);
	    movRemaining -= mov;
	    if (movRemaining > 0) {
	      memmove(newEnd - mov - 1, oldStart + head, movRemaining);
	      head = oldHead + movRemaining;
	    } else
	      head = leadingEmpty - mov;
	    tail = oldTail;
	  } else {//relocate segment of tail to head
	    mov = size - head;
	    if(mov != -startOffset) memmove(newEnd - mov, oldStart + head, mov);
	    memcpy(newEnd - startOffset, oldStart, -startOffset);
	    tail = oldTail;
	    head = size - mov;
	  }
	}
      } else {//data is already contiguous
	if (oldHead >= 0) {
	  if (oldTail < newSize) {//data entirely in common; no move needed
	    head = oldHead;
	    tail = oldTail;
	  } else {//wrap tail around to the beginning
	    tail = oldTail - newSize;
	    memcpy(newStart, newEnd, tail);
	    head = oldHead;
	  }
	} else {//wrap head around to end
	  mov = -oldHead;
	  memcpy(newEnd - mov - 1, oldStart + head, mov);
	  head = size - mov;
	  tail = oldTail;
	}
      }
      outer = head == tail;//if it were empty we weouldn't get here
    } else {//no overlap, much easier, unlikely
      readRaw(newStart, head, used);
      head = 0;
      if (used == newSize) {
	tail = 0;
	outer = true;
      } else {
	tail = used;
	outer = false;
      }
    }
    buf = static_cast<uint8_t*>(newStart);
    size = newSize;
    //this->handleOffset.fetch_add(oldHead - head/*, std::memory_order_seq_cst*/);//??
    this->head.store(head/*, std::memory_order_seq_cst*/);
    this->tail.store(tail/*, std::memory_order_seq_cst*/);
    return RB_SUCCESS;
  }

  RB_PROTO(template<bool R> typename std::enable_if_t<R, uint16_t>)::swallow(uint8_t* newData, size_t dataSize, uint64_t* handleOut, size_t pad) {
    uint16_t ret = RB_SUCCESS;
    if (used() + dataSize > size) ret = resize(used() + dataSize + pad);
    if (!ret) ret = push(newData, dataSize, handleOut);
    return ret;
  }

  RB_PROTO(template<bool R> typename std::enable_if_t<R, uint16_t>)::resize(SIZE_TYPE newSize) {
    return relocate(buf, newSize);//TODO streamline
  }

}
