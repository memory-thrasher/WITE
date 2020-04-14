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
    >no threads are calling relocate
    Any number of threads can read concurrently, though may experience dirty reads on the head
    if another thread is concurrently performing a pop or relocate.
  */
  template<class HeadType, size_t DATA_SIZE = 0, size_t ENTRY_SIZE = sizeof(HeadType),
	   typename SIZE_TYPE = size_t, SIZE_TYPE HeadType::*sizeField = NULL, bool ALLOW_REALLOC = true>
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
  constexpr static bool ADD_SIZE = ((NULL == sizeField) && VARIABLE_RECORD_SIZE);
  constexpr static bool RESIZABLE = (!INLINEDATA && ALLOW_REALLOC);
  constexpr static bool MOVABLE = (!INLINEDATA);
  //size in bytes
  template<bool ID = INLINEDATA, typename std::enable_if_t<!ID, size_t> = 0> RollingBuffer(size_t size, uint8_t* data = NULL);
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
  uint16_t peek(uint8_t* data, size_t* starts, size_t maxsize, size_t max, size_t* pos);
  uint16_t drop();//remove the next element without reading it
  SIZE_TYPE used();
  SIZE_TYPE count();
  SIZE_TYPE freeSpace();
  void readRaw(uint8_t* out, SIZE_TYPE in, SIZE_TYPE size);//careful; only checked for wrap
  void writeRaw(SIZE_TYPE out, uint8_t* in, SIZE_TYPE size);//careful; only checked for wrap
  //NOT thread safe; ensure no threads are doing anything while this is executing
  template<bool M = MOVABLE> typename std::enable_if_t<M, uint16_t> relocate(uint8_t* newhome, SIZE_TYPE newsize);
  template<bool R = RESIZABLE> typename std::enable_if_t<R, uint16_t> resize(SIZE_TYPE size);
  //grow if necessary, plus pad bytes
  template<bool R = RESIZABLE> typename std::enable_if_t<R, uint16_t> swallow(uint8_t* newData, size_t dataSize, uint64_t* handleOut = NULL, size_t pad = 0);
  inline SIZE_TYPE sizeofnext(SIZE_TYPE);
  inline SIZE_TYPE sizeofnext() { return sizeofnext(head); };
  template<SIZE_TYPE cloneSize = DATA_SIZE, SIZE_TYPE cloneCount = 1 + cloneSize / ENTRY_SIZE>
  inline RBIterator<cloneSize, cloneCount> iter();
  private:
  RollingBuffer(const RollingBuffer&) = delete;
  volatile SIZE_TYPE head = 0, tail = 0, size, entries = 0;
  volatile uint8_t* buf;
  bool outer = false;//when head = tail; true means buffer is full, false means empty, otherwise this should be false
  volatile uint8_t inline_data[DATA_SIZE ? DATA_SIZE : 1];
  inline bool wrapped() { return outer || head > tail; }
  public:
  template<SIZE_TYPE cloneSize, SIZE_TYPE cloneCount>
  class RBIterator {//designed to be placed on the thread stack, used as a read buffer for batch reading
  public:
    typedef RollingBuffer<HeadType, DATA_SIZE, ENTRY_SIZE, SIZE_TYPE, sizeField, ALLOW_REALLOC> master_t;
  private:
    SIZE_TYPE i, readHead;
    master_t* master;
    uint8_t data[cloneSize];
    size_t starts[cloneCount];
  public:
    RBIterator(master_t* rb) : master(rb), readHead(rb->head), i(-1) {}
    SIZE_TYPE getHead() { return i ? starts[i - 1] : 0; }
    HeadType* next() {//returns null when out
      HeadType* ret;
      if (i == -1 || !starts[i-1]) {
	if (master->peek(data, starts, cloneSize, cloneCount, &readHead) == RB_BUFFER_UNDERFLOW) return NULL;
	i = 0;
      }
      ret = static_cast<HeadType*>(static_cast<void*>(data + (i ? starts[i-1] : 0)));
      i++;
      return ret;
    }
  };
  };

  //and this is why atomics would work, if they weren't so slow
#define BOUND_INC_HEAD(amnt) {head = head+amnt >= size ? head+amnt-size : head + amnt; outer = false;}
#define BOUND_INC_TAIL(amnt) {tail = tail+amnt >= size ? tail+amnt-size : head + amnt; outer = head == tail;}
#define RB_PROTO(...) template<class HeadType, size_t DATA_SIZE, size_t ENTRY_SIZE, \
			       typename SIZE_TYPE, SIZE_TYPE HeadType::*sizeField, bool ALLOW_REALLOC> \
  __VA_ARGS__ RollingBuffer<HeadType, DATA_SIZE, ENTRY_SIZE, SIZE_TYPE, sizeField, ALLOW_REALLOC>
#define RB_FQN RollingBuffer<HeadType, DATA_SIZE, ENTRY_SIZE, SIZE_TYPE, sizeField, ALLOW_REALLOC>

  RB_PROTO(template<bool ID, typename std::enable_if_t<!ID, size_t> t>)::RollingBuffer(size_t size, uint8_t* data) :
    size(size), buf(data ? data : malloc(size)) {};

  RB_PROTO()::RollingBuffer() : buf(DATA_SIZE ? inline_data : NULL), size(DATA_SIZE) {}

  RB_PROTO()::~RollingBuffer() {
    if (ALLOW_REALLOC) free(buf);
  }

  RB_PROTO(template<SIZE_TYPE cloneSize, SIZE_TYPE cloneCount> RB_FQN::RBIterator<cloneSize, cloneCount>)::iter() {
    RBIterator<cloneSize, cloneCount> ret(this);
    return ret;
  }

  RB_PROTO(SIZE_TYPE)::used() {
    size_t ret;
    ret = head == tail ? (outer ? size : 0) : head < tail ? tail - head : tail + size - head;
    return ret;
  }

  RB_PROTO(SIZE_TYPE)::count() {
    return entries;
  }

  RB_PROTO(SIZE_TYPE)::freeSpace() {
    return size - used();
  }

  RB_PROTO(void)::readRaw(uint8_t* out, SIZE_TYPE offset, SIZE_TYPE readSize) {
    if (offset >= size) offset -= size;
    if (offset + readSize >= size) {//:(
      memcpy(out, buf[offset], size - offset);
      memcpy(out[size - offset], buf, readSize + offset - size);
    } else {
      memcpy(out, buf[offset], readSize);
    }
  }

  RB_PROTO(void)::writeRaw(SIZE_TYPE offset, uint8_t* in, SIZE_TYPE writeSize) {
    if (offset >= size) offset -= size;
    if (offset + writeSize >= size) {//:(
      memcpy(buf[offset], in, size - offset);
      memcpy(buf, in[size - offset], writeSize + offset - size);
    } else {
      memcpy(buf[offset], in, writeSize);
    }
  }

  RB_PROTO(SIZE_TYPE)::sizeofnext(SIZE_TYPE h) {
    SIZE_TYPE offset;
    if (ENTRY_SIZE) return ENTRY_SIZE;
    offset = ADD_SIZE ? offsetof(HeadWithSizeType, size) : static_cast<size_t>(sizeField);
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
    if (handleOut) *handleOut = tail;
    writeRaw(tail, dataSize, sizeof(SIZE_TYPE));
    writeRaw(tail + sizeof(SIZE_TYPE), newData, dataSize + sizeof(HeadType));
    BOUND_INC_TAIL(dataSize + sizeof(HeadWithSizeType));
    entries++;
    return RB_SUCCESS;
  }

  RB_PROTO(template<bool AS> typename std::enable_if_t<AS, uint16_t>)::pushBlocking(HeadType* newData, SIZE_TYPE dataSize, size_t* handleOut) {
    while(dataSize + sizeof(HeadWithSizeType) > freeSpace());//TODO yield?
    if (handleOut) *handleOut = tail;
    writeRaw(tail, dataSize, sizeof(SIZE_TYPE));
    writeRaw(tail + sizeof(SIZE_TYPE), newData, dataSize + sizeof(HeadType));
    BOUND_INC_TAIL(dataSize + sizeof(HeadWithSizeType));
    entries++;
    return RB_SUCCESS;
  }

  RB_PROTO(template<bool AS> typename std::enable_if_t<!AS, uint16_t>)::push(HeadType* newData, size_t* handleOut) {
    SIZE_TYPE dataSize = sizeof(HeadType) + newData->*sizeField;
    if (dataSize + sizeof(HeadType) > freeSpace()) return RB_BUFFER_OVERFLOW;
    if (handleOut) *handleOut = tail;
    writeRaw(tail, dataSize, dataSize);
    BOUND_INC_TAIL(dataSize);
    entries++;
    return RB_SUCCESS;
  }

  RB_PROTO(template<bool AS> typename std::enable_if_t<!AS, uint16_t>)::pushBlocking(HeadType* newData, size_t* handleOut) {
    SIZE_TYPE dataSize = sizeof(HeadType) + newData->*sizeField;
    while(dataSize + sizeof(HeadType) > freeSpace());//TODO yield?
    if (handleOut) *handleOut = tail;
    writeRaw(tail, dataSize, dataSize);
    BOUND_INC_TAIL(dataSize);
    entries++;
    return RB_SUCCESS;
  }

  RB_PROTO(uint16_t)::pop(uint8_t* data, size_t* starts, size_t maxsize, size_t max) {
    SIZE_TYPE subsize, totalWrote = 0;
    uint16_t ret;
    if (!entries) return RB_BUFFER_UNDERFLOW;
    ret = RB_SUCCESS;
    if (entries > max) ret = RB_READ_INCOMPLETE;
    else max = entries;
    while (max && (subsize = sizeofnext() + sizeof(HeadType)) && subsize <= maxsize) {
      readRaw(data, head, subsize);
      entries--;
      BOUND_INC_HEAD(subsize + sizeof(SIZE_TYPE));
      max--;
      maxsize -= subsize;
      data += subsize;
      *starts = totalWrote += subsize;
      starts++;
    }
    if (max) ret = RB_READ_INCOMPLETE;
    return ret;
  }

  RB_PROTO(uint16_t)::peek(uint8_t* data, size_t* starts, size_t maxsize, size_t max, size_t* pos) {
    SIZE_TYPE subsize, totalWrote = 0, subpos = head;
    uint16_t ret;
    if (!entries) return RB_BUFFER_UNDERFLOW;
    if (!pos) pos = subpos;
    ret = RB_SUCCESS;
    if (entries > max) ret = RB_READ_INCOMPLETE;
    else max = entries;
    while (max && subsize = sizeofnext() + sizeof(HeadType) && subsize <= maxsize) {
      readRaw(data, *pos, subsize);
      entries--;
      *pos += subsize + sizeof(SIZE_TYPE);
      if (*pos >= size) *pos -= size;
      max--;
      maxsize -= subsize;
      data += subsize;
      *starts = totalWrote += subsize;
      starts++;
    }
    if (max) ret = RB_READ_INCOMPLETE;
    return ret;
  }

  //NOT thread safe; ensure no threads are doing anything while this is executing; also invalidates all handles
  RB_PROTO(template<bool M> typename std::enable_if_t<M, uint16_t>)::relocate(uint8_t* newStart, SIZE_TYPE newSize) {
    uint8_t* newEnd, *oldStart, *oldEnd;
    size_t used = this->used(), leadingEmpty, trailingEmpty, mov;
    int64_t oldHead, oldTail, startOffset, movRemaining;
    bool wasWrapped;
    if (newSize < used) return RB_BUFFER_OVERFLOW;
    newEnd = newStart + newSize;
    oldStart = static_cast<uint8_t*>(buf);
    oldEnd = oldStart + size;
    if (!used) {//nothing to move; ideal
      outer = head = tail = 0;
    } else if(std::less<void*>()(newStart, oldEnd) && std::less<void*>()(oldStart, newEnd)) {//overlap, common
      wasWrapped = outer || tail < head;
      startOffset = static_cast<int64_t>(oldStart - newStart);
      oldHead = head + startOffset;//in new space
      oldTail = tail + startOffset;
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
	  } else {//case: newend < oldend: trailingEmpty = 0, must relocate segment of head at end to beginning
	    mov = static_cast<size_t>(oldEnd - newEnd);
	    if (mov != startOffset) memmove(newStart + mov, oldStart, tail);
	    memcpy(newStart, newEnd, mov);
	    tail = newStart + mov + tail;
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

#undef INLINEDATA
#undef VARIABLE_RECORD_SIZE
#undef ADD_SIZE
#undef RESIZABLE
#undef MOVABLE

