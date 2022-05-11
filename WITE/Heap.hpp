namespace WITE::Collections {

  /*
   * Collection of untyped data of arbitrary length. Cheap, fast, dirty storage, designed for mutex-free multi-thread simultaneous read and write. Must be reset at regular intervals (generally once per frame); objects can only be removed all at once. Concurrent read/write is thread-safe but write/write is not. Caller is responsible for re-associating the type of data within.
   */
  class Heap {
  private:
    std::unique_ptr<uint8_t[]> data;
    size_t dataSize;
    std::atomic_size_t allocationHead, readHead;
  public:
    class MemberIterator : public std::iterator<std::forward_iterator_tag, Member> {
      Heap* via;
      Member head;
    public:
      MemberIterator() : via(NULL), head({NULL, 0});
      MemberIterator(Heap* src) : via(src), head(src->read()) {};
      MemberIterator(const MemberIterator& src) : via(src.via), head(src.head) {};
      MemberIterator& operator++() { head = via->read(head); };
      bool operator==(const MemberIterator& rhs) const { return head.start == rhs.head.start; };
      bool operator!=(const MemberIterator& rhs) const { return head.start != rhs.head.start; };
      Member& operator*() { return head; };
    };
    class HeapWriteStream : public WITE::Collections::WriteStream {
    private:
      Heap* heap;
      size_t len;
      size_t* lenOut;
      void* out;
    public:
      HeapWriteStream(Heap* dst);
      size_t write(void* src, size_t maxlen);
      void close();
    }
    typedef struct Member {
      uint8_t* start;
      size_t len;
    } Member;
    Heap(size_t initialSize);
    void reset(size_t newSize = 0);
    const Member& write(const uint8_t* src, size_t len);
    const Member& read(const Member& previous);
    const Member& read();//first
    MemberIterator begin() { return MemberIterator(this); };
    static MemberIterator end() { return MemberIterator(); };
    HeapWriteStream& writeStream();
  }

}
