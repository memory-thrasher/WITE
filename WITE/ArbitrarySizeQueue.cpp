#include "ArbitrarySizeQueue.hpp"
#include "SyncLock.hpp"

namespace WITE::Collections {

  ArbitrarySizeQueue::ArbitrarySizeQueue(bool makeThreadSafe, size_t allocationBlockSize, size_t initialBlockCount = 1) : threadSafe(makeThreadSafe) {
    WITE::Util::ScopeLock lock(threadSafe ? lock : NULL);
    for(size_t i = 0;i < allocationBlockSize;i++) {
      grow();
    }
  }

  void ArbitrarySizeQueue::grow() {
    //internal only, we're already locked
    buckets.push_back(new uint8_t[bucketSize]);
  }

  void ArbitrarySizeQueue::advanceBucket() {
    //internal only, we're already locked
    head.startIdx = 0;
    head.startBucketId++;
    while(head.startBucketId - recycledBuckets >= buckets.size())
      grow();
  }

  void ArbitrarySizeQueue::doWrite(const uint8_t* src, size_t len, size_t offsetInSrc = 0) {
    //internal only, we're already locked
    std::memcpy(reinterpret_cast<void*>(&buckets[head.startBucketId - recycledBuckets][head.startIdx]),
	   reinterpret_cast<void*>(src) + offsetInSrc,
	   len);
    head.startIdx += len;
  }

#define doWriteCast(src, ...) doWrite(reinterpret_cast<uint8_t*>(src), __VA_ARGS__)

  const pseudoPointer& ArbitrarySizeQueue::push(const uint8_t* src, size_t len) {
    WITE::Util::ScopeLock lock(threadSafe ? lock : NULL);
    pseudoPointer ret = head;
    ret.len = len;
    size_t thisWrite = std::min(sizeof(len), bucketSize - head.startIdx);
    doWriteCast(&len, sizeof(len));
    if(thisWrite < sizeof(len)) {
      advanceBucket();
      doWriteCast(&len, sizeof(len) - thisWrite, thisWrite);
    }
    thisWrite = std::min(len, bucketSize - head.startIdx);
    doWrite(src, thisWrite);
    len -= thisWrite;
    src += thisWrite;
    while(len) {
      advanceBucket();
      thisWrite = std::min(len, bucketSize - head.startIdx);
      doWrite(src, thisWrite);
      len -= thisWrite;
      src += thisWrite;
    }
    return ret;
  }

  void doRead(uint8_t* dst, size_t len, const pseudoPointer* target, size_t skip = 0) {
    size_t thisRead;
    if(skip < bucketSize - target.startIdx) {
      [[likely]]
      thisRead = std::min(len, bucketSize - target.startIdx - skip);
      std::memcpy(reinterpret_cast<void*>(dst),
		  reinterpret_cast<void*>(&buckets[target.startBucketId - bucketsRecycled][target.startIdx + skip]),
		  thisRead);
      len -= thisRead;
      skip = 0;
      dst += thisRead;
    } else {
      skip -= bucketSize - target.startIdx;
    }
    size_t readTargetIdx = skip / bucketSize, readTargetBucket = skip % bucketSize;
    while(len) {
      [[unlikely]]
      thisRead = std::min(len, bucketSize);
      std::memcpy(reinterpret_cast<void*>(dst),
		  reinterpret_cast<void*>(&buckets[readTargetBucket][readTargetIdx]),
		  thisRead);
      dst += thisRead;
      len -= thisRead;
      readTargetIdx = 0;
      readTargetBucket++;
    }
  }

#define doReadCast(dst, ...) doRead(reinterpret_cast<uint8_t*>(dst), __VA_ARGS__)

  size_t pop(size_t outMaxLen, uint8_t* out); {
    WITE::Util::ScopeLock lock(threadSafe ? lock : NULL);
    size_t realLen;
    doReadCast(&readLen, &tail, sizeof(realLen));
    if(!outMaxLen) return realLen;
    if(realLen < outMaxLen) return 0;
    doRead(out, &tail, realLen, sizeof(realLen));
    size_t newOffset = realLen + tail.startIdx;
    size_t recycledBuckets = newOffset / bucketSize;
    tail.startIdx = newOffset % bucketSize;
    tail.startBucketId += recycledBuckets;
    this.bucketsRecycled += recycledBuckets;
    for(size_t i = 0;i < recycledBuckets;i++) {
      buckets.push_back(buckets.pop_front());
    }
    return realLen;
  }

  size_t read(const pseudoPointer* target, size_t start, size_t len, void* buffer_out) {
    WITE::Util::ScopeLock lock(threadSafe ? lock : NULL);
    size_t realLen;
    doReadCast(&realLen, target, sizeof(realLen));
    if(!len) return realLen;
    len = std::min(len, realLen);
    doRead(out, target, len, sizeof(realLen));
    return len;
  }

  bool isEmpty() {
    WITE::Util::ScopeLock lock(threadSafe ? lock : NULL);
    return memcmp(&head, &tail, sizeof(pseudoPointer)) == 0;
  }

}
