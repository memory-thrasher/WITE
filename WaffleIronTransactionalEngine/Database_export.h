#pragma once

namespace WITE {

  struct strcmp_t {
    bool operator() (const std::string& a, const std::string& b) const { return a.compare(b) < 0; };
  };

  class export_def Database {
  public:
    typedef uint64_t Entry;
    typedef uint16_t type;
    typedef WITE::Callback_t<uint64_t, Entry, void*>* receiver_t;
    typedef Callback_t<void, Entry>* handle_t;
    typedef Callback_t<void, Entry, Entry*>* initHandle_t;
    typedef struct {
      handle_t update;
      initHandle_t init;
      handle_t destroy;
    } typeHandles;
    typedef struct precompiledBatch_entry_t {
      constexpr precompiledBatch_entry_t(const size_t start, const size_t len) : start(start), len(len) {}
      size_t start, len;
    } precompiledBatch_entry;
    template<size_t size> using precompiledBatch_t = std::array<precompiledBatch_entry, size>;
    constexpr static Entry NULL_ENTRY = std::numeric_limits<Entry>::max();
    constexpr static size_t NULL_OFFSET = std::numeric_limits<size_t>::max();
    constexpr static uint64_t NULL_FRAME = std::numeric_limits<uint64_t>::max();
    constexpr static size_t ALLOCATION_MAP_SIZE = 32;
  protected:
    constexpr static size_t BLOCKSIZE = 4096;
    enum state_t : uint16_t { unallocated = 0, data, branch, trunk };//branches contain references to datas, trunks contain references to branches or other trunks
    enum logEntryType_t : uint8_t { illegal = 0, del, update };//so fresh memset(0) space is easier to spot as invalid
    typedef struct {
      state_t state;
      type type;//global enum-esque maintained by consumer, used by getEntriesOfType. [0,127] reserved
    } _entry;
  public:
    constexpr static size_t BLOCKDATASIZE = BLOCKSIZE - sizeof(_entry);
  protected:
    constexpr static size_t MAXPUTSIZE = BLOCKSIZE;//should be indexable by (enqueued)LogEntry::size
    constexpr static size_t MAXBLOCKPOINTERS = (BLOCKDATASIZE - sizeof(uint32_t)) / sizeof(Entry);//510
    typedef struct {
      uint32_t subblockCount;
      Entry subblocks[MAXBLOCKPOINTERS];
    } blocklistData;
    typedef struct {
      union {
        uint8_t raw[sizeof(_entry)];
        _entry header;
      };
      union {
	uint8_t data[BLOCKDATASIZE];
	blocklistData children;
      };
    } loadedEntry;
    typedef struct {//smaller is better here
      uint64_t frameIdx;
      logEntryType_t type;
      uint16_t size;
      uint64_t offset;//offset into entity or its children that will be overwritten when this log is applied
      Entry start;
    } enqueuedLogEntry;//size=0x20 (8+?+2+8+8=20)
    typedef struct {
      uint64_t frameIdx;
      logEntryType_t type;
      size_t size;
      uint64_t offset;//offset into entry that will be overwritten when this log is applied
      size_t nextForObject;//-1 for none
      Entry start;
    } logEntry;
    typedef struct {
      size_t score;
      Entry ent;
    } cacheIndex;
    typedef struct {
      uint64_t lastUseFrame;
      Entry entry;
      uint32_t syncstate;//mostly pad for now, future expansion, bitmask of CACHESYNCSTATE_*
      uint32_t readsCachedLifetime;
      loadedEntry e;
    } cacheEntry;
    typedef struct allocationTableEntry_t {
      constexpr static size_t OBJECT_NULL = ((size_t)0)-1;
      cacheEntry* cacheLocation;//can be null
      uint64_t lastWrittenFrame, nextWriteFrame;
      size_t tlogHeadForObject, tlogTailForObject;//can be -1 for none
      state_t allocationState;//as of now, used for allocate/free, which might happen on the same frame but that's ok because they're managed by prime thread
      //empty space 6 bytes
      uint32_t readsThisFrame, readsLastFrame;//reset to 0 by master allocator, read to decide if it should be cached, dirty increment by other threads upon *disk* read
      Entry typeListNext, typeListLast;//linked list of all objects of this type, rebuilt on reload
      Object* obj;//passed to all functions, only meaningful in root Entrys
    } allocationTableEntry;//size: 80 bytes
    typedef struct threadResource_t {
      typedef RollingBuffer<enqueuedLogEntry, uint16_t, 65535, 0, (uint16_t)offsetof(enqueuedLogEntry, size), false> transactionalBacklog_t;
      transactionalBacklog_t transactionalBacklog;
      uint64_t currentFileIdx = -1;//target if it exists, active otherwise
      FILE *activeFile, *targetFile;
      volatile Entry allocRet = NULL_ENTRY;
      volatile size_t allocSize = 0;
      Entry * volatile map;
    } threadResource;
    typedef typeHandles perTypeStatic;
    typedef struct {
      Entry firstOfType, lastOfType;//linked list
    } perType;
  public:
    static std::unique_ptr<Database> makeDatabase(const char * filenamefmt, size_t cachesize, int64_t loadidx = -1);//filenamefmt must contain %d
    static std::unique_ptr<Database> makeDatabase(size_t cachesize);//temp db, no file backing, for static content like the main menu or splash
    ~Database() = default;
    virtual void gracefulStop() = 0;
    virtual void rebase() = 0;//advance index on current template
    virtual void rebase(const char *, int64_t startidx) = 0;//save
    template<class T> Entry allocate(type);
    virtual void free(Entry) = 0;
    virtual void advanceFrame() = 0;//only called by master thread between renders
    virtual size_t getEntriesOfType(type type, Entry* out, size_t maxout) = 0;//returns number of entries
    template<class T, class... Zs> constexpr static auto createPrecompiledBatch(Zs... z);
    //puts
    virtual void put(Entry, const uint8_t * in, uint64_t start, uint16_t len) = 0;//out starts with the bit that is updated
    virtual void put(Entry, const uint8_t * in, uint64_t* starts, uint64_t* lens, size_t count) = 0;//in starts at beginning of entry data
    template<size_t fields, precompiledBatch_t<fields> batch> void put(Entry, const uint8_t * in);
    template<class T> void put(Entry, const T * in);//everything, avoid this
    template<class T, class U> void put(Entry, const T * in, U T::*field);
    template<class T, class U, class... Zs> inline void put(Entry e, const T * in, U T::*u, Zs... z);//use precompiled batches instead
    //gets
    virtual void get(Entry, uint8_t* out, uint64_t start, uint16_t len) = 0;//out starts with the bit that is fetched
    virtual void get(Entry, uint8_t* out, uint64_t* starts, uint64_t* lens, size_t count) = 0;//out starts at beginning of entry data
    // void get(Entry, uint8_t* out, const precompiledBatch_t*);//TODO//
    template<class T> void get(Entry, T* out);//everything, general use
    template<class T, class U> void get(Entry, T* out, U T::*field);
    template<class T, class U, class V, class... Zs> void get(Entry, T* out, U T::*u, V T::*v, Zs... z);
    virtual Entry getChildEntryByIdx(Entry root, size_t idx) = 0;
    //inter object communication bs
    //static void registerReceiver(const std::string, receiver_t);
    static void registerType(type t, typeHandles funcs);
    //uint64_t sendMessage(const std::string, const Entry receiver, void* data);
    //uint64_t sendMessageToAll(const std::string, const type receivers, void* data);
    virtual type getEntryType(Entry e) = 0;
    static typeHandles* getHandlesOfType(type t);
    virtual void getEntriesWithUpdate(std::vector<Entry>* out) = 0;//pulled by entry in update
    virtual Object** getObjectInstanceFor(Entry e) = 0;
    virtual uint64_t getCurrentFrame() = 0;
  protected:
    Database() = default;
    Database(const Database&) = delete;//no. just no.
    constexpr static uint8_t zero[BLOCKSIZE] = { 0 };
    template<size_t idx, size_t count, class T, class U, class V, class... Zs> constexpr static void createPrecompiledBatch(precompiledBatch_t<count>& out, U T::*u, V T::*v, Zs... z);
    template<size_t idx, size_t count, class T, class U> constexpr static void createPrecompiledBatch(precompiledBatch_t<count>& out, U T::*u);
    virtual Entry allocate(size_t size, type) = 0;
  };

  template<class T> Database::Entry Database::allocate(type t) {
    return allocate(sizeof(T), t);
  }

  template<size_t idx, size_t count, class T, class U, class V, class... Zs>
  constexpr void Database::createPrecompiledBatch(precompiledBatch_t<count>& out, U T::*u, V T::*v, Zs... z) {
    createPrecompiledBatch<idx>(out, u);//to specialization
    createPrecompiledBatch<idx+1>(out, v, std::forward<Zs>(z)...);//recurse or specilization
  }

  template<size_t idx, size_t count, class T, class U>
  constexpr void Database::createPrecompiledBatch(precompiledBatch_t<count>& out, U T::*u) {
    out.data[idx] = precompiledBatch_entry((size_t)u, sizeof(U));
  }

  template<class T, class... Zs>
  constexpr auto Database::createPrecompiledBatch(Zs... z) {
    constexpr size_t len = sizeof...(Zs);
    auto simpleArray = precompiledBatch_t<len>();
    createPrecompiledBatch<len, 0, T, Zs...>(&simpleArray, std::forward<Zs>(z)...);//populate
    size_t contiguousCount = 0;
    for(size_t i = 0;i < len-1;i++) {
      if(simpleArray[i+1].start == simpleArray[i].start + simpleArray[i].len)
	contiguousCount++;
    }
    auto ret = precompiledBatch_t<len - contiguousCount>();
    size_t j = 0;
    for(size_t i = 0;i < len;j++) {
      size_t start = simpleArray[i].start;
      size_t len = simpleArray[i].len;
      i++;
      while(i < len && simpleArray[i].start == start + len) {
	len += simpleArray[i].len;
	i++;
      }
      ret[j] = precompiledBatch_entry(start, len);
    }
    return ret;
  }

  template<size_t batchSize, Database::precompiledBatch_t<batchSize> batch> void Database::put(Entry t, const uint8_t * inRaw) {
    uint64_t start, end, i;
    uint64_t len;
    for(i = 0;i < batchSize;i++) {
      put(t, inRaw + batch[i].start, batch[i].start, (uint16_t)batch[i].len);
    }
  }

  template<class T> void Database::put(Entry e, const T * in) {//everything, avoid this
    uint64_t size = sizeof(T), zero = 0;
    put(e, static_cast<uint8_t*>(in), &zero, &size, 1);
  }

  template<class T, class U> void Database::put(Entry e , const T * in, U T::*field) {
    uint64_t size = sizeof(U), offset = size_t(field);
    put(e, static_cast<uint8_t*>(in), &offset, &size, 1);
  }

  template<class T, class U, class... Zs> void Database::put(Entry e, const T * in, U T::*u, Zs... z) {
    put(e, in, u);
    put(e, in, z...);//recurse
  }

  template<class T> void Database::get(Entry e, T* out) {
    get(e, reinterpret_cast<uint8_t*>(out), 0, sizeof(T));
  }

  template<class T, class U> void Database::get(Entry e, T* out, U T::*field) {
    get(e, reinterpret_cast<uint8_t*>(out), size_t(field), sizeof(U));
  }

  template<class T, class U, class V, class... Zs> void Database::get(Entry e, T* out, U T::*u, V T::*v, Zs... z) {
    get<T, U>(e, out, u);
    get<T, V, Zs...>(e, u, v, std::forward<Zs>(z)...);
  }
  
}
