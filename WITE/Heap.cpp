#include "Heap.hpp"

namespace WITE::Collections {

  Heap::Heap(size_t initialSize) :
    data(std::make_unique(initialSize)),
    dataSize(initialSize),
    allocationHead(0),
    readHead(0) {}

  void Heap::reset(size_t newSize = 0) {
    allocationHead = readHead = 0;
    if(newSize) { //resize
      initialSize = newSize;
      data = std::make_unique(initialSize);
    }
  }

  const Member& Heap::write(const uint8_t* src, size_t len) {
    size_t size = len + sizeof(size_t);
    size_t dst_idx = allocationHead.fetch_add(size);
    if(dst_idx + size >= dataSize) {
      allocationHead -= size;
      throw new std::runtime_error("Heap overflow (write past end)");
    }
    uint8_t* dst = data.get() + dst_idx;
    memcpy(dst, reinterpret_cast<void*>(&len), sizeof(size_t));
    memcpy(dst + sizeof(size_t), src, len);
    readHead += size;
    return { dst, len };
  }

  const Member& read() {
    return { data.get() + sizeof(size_t), reinterpret_cast<size_t*>(data.get())[0] };
  }

  const Member& read(const Member& prev) {
    uint8_t* target = prev.data + prev.length;
    if(WITE::Config::valildate_memory && (prev.data < data.get() || target >= data.get() + dataSize)) {
      throw new std::runtime_error("Heap underflow (read illegal target)");
    }
    if(target > data.get() + readHead) return { NULL, 0 };
    return { target + sizeof(size_t_), reinterpret_cast<size_t*>(target)[0] };
  }

  HeapWriteStream::HeapWriteStream(Heap* dst) : heap(dst), len(0) {
    lenOut = reinterpret_cast<size_t*>(heap->data.get() + heap->allocationHead);
    out = reinterpret_cast<void*>(lenOut + 1);
    heap->allocationHead += sizeof(size_t);
  }

  size_t HeapWriteStream::write(void* src, size_t len) {
    heap->allocationHead += len;
    out += len;
    this.len += len;
    if(heap->allocationHead >= heap->dataSize) {
      heap->allocationHead -= this.len;
      throw new std::runtime_error("Heap overflow (write past end)");
    }
    memcpy(out, src, len);
    return len;
  }

  void HeapWriteSteam::close() {
    *lenOut = len;
  }

  HeapWriteStream& Heap::writeStream() {
    return HeapWriteStream(this);
  }

}
