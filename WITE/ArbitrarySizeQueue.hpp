namespace WITE::Collections {

  /*
   * Queue of untyped data of arbitrary length. Dynamically expands via one-off malloc as needed. Does not directly support pointers (because stored data may not be contiguous), but includes a pointer-like struct that serves as a stand-in. Pseudo-pointers are only invalidated when the pointed-to data is popped. Iterators are not supported. Push and pop are the only write operations. Pseudo-pointers, peekand pop are the only read operations. Thread safe if requested via atomic non-mutex.
   *
   * Internally, whenever a bucket is emptied, it is moved to the end of the buckets collection, and the recycledBuckets counter is incremented. recycledBuckets is subtracted from any externally-provided pseudoPointer to produce the actual index within the container, thus pseudoPoints are not invalidated when a bucket is recycled.
   */
  class ArbitrarySizeQueue {
  public:
    public typedef struct pseudoPointer {
      size_t startBucketId, startIdx, len;
    } pseudoPointer;
  private:
    const bool threadSafe;
    std::dequeue<uint8_t[]> buckets;
    pseudoPointer head, tail;
    WITE::Util::SyncLock lock;
    size_t bucketSize, bucketsRecycled;
    void grow();
    void advanceBucket();
    void doWrite(const uint8_t* src, size_t len, size_t offsetInSrc = 0);
    void doRead(uint8_t* dst, size_t len, const pseudoPointer* target, size_t skip = 0);
  public:
    ArbitrarySizeQueue(bool makeThreadSafe, size_t allocationBlockSize, size_t initialBlockCount = 1);
    const pseudoPointer& push(const uint8_t* src, size_t len);
    size_t pop(size_t outMaxLen, uint8_t* out);
    size_t read(const pseudoPointer* target, size_t start, size_t len, void* buffer_out);
    bool isEmpty();
  }

}
