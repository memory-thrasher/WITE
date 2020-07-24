#pragma once

#include "Thread.h"
#include "RollingBuffer.h"
#include "Transform.h"

namespace WITE {

  class Object;

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
    typedef struct {
      size_t start, len;
    } precompiledBatch_entry;
    typedef struct {
      std::unique_ptr<precompiledBatch_entry[]> data;
      size_t size;
    } precompiledBatch_t;
    constexpr static Entry NULL_ENTRY = std::numeric_limits<Entry>::max();
    constexpr static size_t NULL_OFFSET = std::numeric_limits<size_t>::max();
    constexpr static uint64_t NULL_FRAME = std::numeric_limits<uint64_t>::max();
    constexpr static size_t ALLOCATION_MAP_SIZE = 32;
    private:
    constexpr static size_t BLOCKSIZE = 4096;
    enum state_t : uint16_t { unallocated = 0, data, branch, trunk };//branches contain references to datas, trunks contain references to branches or other trunks
    enum logEntryType_t : uint8_t { del = 0, update };
    typedef struct {
      state_t state;
      type type;//global enum-esque maintained by consumer, used by getEntriesOfType. [0,127] reserved
    } _entry;
    public:
    constexpr static size_t BLOCKDATASIZE = BLOCKSIZE - sizeof(_entry);
    private:
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
    typedef struct {
      constexpr static size_t OBJECT_NULL = ((size_t)0)-1;
      cacheEntry* cacheLocation;//can be null
      uint64_t lastWrittenFrame, nextWriteFrame;
      size_t tlogHeadForObject, tlogTailForObject;//can be -1 for none
      state_t allocationState;//as of now, used for allocate/free, which might happen on the same frame but that's ok because they're managed by prime thread
      //empty space 6 bytes
      uint32_t readsThisFrame, readsLastFrame;//reset to 0 by master allocator, read to decide if it should be cached, dirty increment by other threads upon *disk* read
      Entry typeListNext, typeListLast;//linked list of all objects of this type, rebuilt on reload
      class Object* obj;//passed to all functions, only meaningful in root Entrys
    } allocationTableEntry;//size: 80 bytes
    typedef struct {
      typedef RollingBuffer<enqueuedLogEntry, uint16_t, 65535, 0, (uint16_t)offsetof(enqueuedLogEntry, size), false> transactionalBacklog_t;
      transactionalBacklog_t transactionalBacklog;
      uint64_t currentFileIdx;//target if it exists, active otherwise
      FILE *activeFile, *targetFile;
      volatile Entry allocRet = NULL_ENTRY;
      volatile size_t allocSize = 0;
      Entry * volatile map;
    } threadResource_t;
    typedef typeHandles perTypeStatic;
    typedef struct {
      Entry firstOfType, lastOfType;//linked list
    } perType;
    public:
    Database(const char * filenamefmt, size_t cachesize, int64_t loadidx = -1);//filenamefmt must contain %d
    Database(size_t cachesize);//temp db, no file backing, for static content like the main menu or splash
    ~Database();
    void rebase();//advance index on current template
    void rebase(const char *, int64_t startidx);//save
    template<class T> Entry allocate(type);
    void free(Entry);
    void advanceFrame();//only called by master thread between renders
    size_t getEntriesOfType(type type, Entry* out, size_t maxout);//returns number of entries
    template<class T, class... Zs> constexpr static auto createPrecompiledBatch(Zs... z);
    //puts
    void put(Entry, const uint8_t * in, uint64_t start, uint16_t len);//out starts with the bit that is updated
    void put(Entry, const uint8_t * in, uint64_t* starts, uint64_t* lens, size_t count);//in starts at beginning of entry data
    void put(Entry, const uint8_t * in, const precompiledBatch_t*);
    template<class T> void put(Entry, const T * in);//everything, avoid this
    template<class T, class U> void put(Entry, const T * in, U T::*field);
    template<class T, class U, class... Zs> inline void put(Entry e, const T * in, U T::*u, Zs... z);//use precompiled batches instead
    //gets
    void get(Entry, uint8_t* out, uint64_t start, uint16_t len);//out starts with the bit that is fetched
    void get(Entry, uint8_t* out, uint64_t* starts, uint64_t* lens, size_t count);//out starts at beginning of entry data
    // void get(Entry, uint8_t* out, const precompiledBatch_t*);//TODO//
    template<class T> void get(Entry, T* out);//everything, general use
    template<class T, class U> void get(Entry, T* out, U T::*field);
    template<class T, class U, class V, class... Zs> void get(Entry, T* out, U T::*u, V T::*v, Zs... z);
    Entry getChildEntryByIdx(Entry root, size_t idx);
    //inter object communication bs
    //static void registerReceiver(const std::string, receiver_t);
    static void registerType(type t, typeHandles funcs);
    //uint64_t sendMessage(const std::string, const Entry receiver, void* data);
    //uint64_t sendMessageToAll(const std::string, const type receivers, void* data);
    type getEntryType(Entry e);
    static typeHandles* getHandlesOfType(type t);
    inline void getEntriesWithUpdate(std::vector<Entry>* out);//pulled by entry in update
    Object** getObjectInstanceFor(Entry e);
    uint64_t getCurrentFrame();
    private:
    constexpr static uint8_t zero[BLOCKSIZE] = { 0 };
    template<size_t idx, class T, class U, class V, class... Zs>
    constexpr static void createPrecompiledBatch(precompiledBatch_t& out, U T::*u, V T::*v, Zs... z);
    template<size_t idx, class T, class U> constexpr static void createPrecompiledBatch(precompiledBatch_t& out, U T::*u);
    Database(const Database&) = delete;//no. just no.
    void push(enqueuedLogEntry*);
    Entry allocate(size_t size, Entry* allocationMap = NULL);
    void getRecurse(Entry, uint8_t* out, uint64_t* starts, uint64_t* lens, size_t count, size_t* offsetOfE, size_t* regionIdx);
    template<typename T> T getRaw(Entry e, size_t offset);//e contains offset
    template<typename T> T getRaw(Entry e, size_t offset, uint64_t frame);//e contains offset
    void getRaw(Entry e, size_t offset, size_t size, uint8_t* out, uint64_t frame);//e contains offset, out starts at beginning of returned data
    state_t getEntryState(Entry e);
    const char * filenamefmt;
    volatile uint64_t filenameIdx, fileIdx = 0;
    std::string active, target;
    //vv these three share one giant malloc: "primeRam", and adapt at runtime to share it.
    //This is most of the game ram.
    volatile union { allocationTableEntry* allocTab; void* allocTabRaw; uint8_t* allocTabByte; };//One per loadedEntry in existance.
    //volatile void* tlog;//regions of varying size each start with a logEntry//this is now managed by tlogManager
    volatile union { cacheEntry* cache; void* cacheRaw; };
    size_t primeRamSize, atCount, tlogSize, cacheCount;
    //??std::vector<std::vector<Entry>> allocated;//transient, one inner vector per object type
    uint64_t currentFrame, targetFrame = 0;//transient, resets on load
    //static std::map<std::string, receiver_t, strcmp_t> messageReceivers;
    static std::vector<type> typesWithUpdates;//only edited on static init, pre multithread
    static std::map<type, perTypeStatic> typesStatic;
    std::map<type, perType> types;//on current frame
    RollingBuffer<logEntry, size_t, 0, 0, offsetof(logEntry, size), false> tlogManager;//TODO index of free?
    std::atomic_uint8_t masterThreadState;
    std::atomic_uint8_t dbStatus;
    ThreadResource<threadResource_t> threadResources;
    //prime thread only: (all transient)
    constexpr static size_t MAX_BATCH_FLUSH = 32;
    constexpr static size_t BUFFER_SIZE = 65536;
    size_t pt_starts[MAX_BATCH_FLUSH], pt_writenBytesThisFrame, pt_writenBytesLastFrame, pt_freeSpace;
    uint64_t pt_lastWriteFrame;
    FILE* pt_activeF, *pt_targetF;
    union {
      uint8_t raw[BUFFER_SIZE];
      loadedEntry entry;
      struct {
	uint8_t padToLoglen[sizeof(logEntry) - sizeof(enqueuedLogEntry)];//pad so data lines up with log data
	union {
	  uint8_t raw[1];
	  struct {
	    enqueuedLogEntry header;
	  };
	};
      } enqueuedLog;
      struct {
	logEntry header;
	uint8_t data[1];
      } log;
    } pt_buffer;
    cacheIndex* pt_cacheStaging;
    size_t pt_cacheStagingSize;
    void primeThreadEntry();
    Entry pt_allocate(size_t size, Entry* map);
    bool pt_adoptTlog(threadResource_t::transactionalBacklog_t* src);
    bool pt_commitTlog();
    bool pt_flushTlog(uint64_t minimumFreeSpace = 0);//commit from previous frames, until has specified free space (flush all if given 0)
    bool pt_mindSplitBase();
    void pt_rethinkLayout(size_t requiredNewEntries = 0);
    void pt_rethinkCache();
    void pt_push(logEntry*);
    };

  template<class T> Database::Entry Database::allocate(type t) {
    //TODO create and return allocation map, pass to init and then object::make to locate applicable data subsets (including transform)
    Entry allocationMap[ALLOCATION_MAP_SIZE];
    Entry ret = allocate(sizeof(T), allocationMap);
    put(ret, reinterpret_cast<uint8_t*>(&t), offsetof(loadedEntry, header.type) - sizeof(loadedEntry), sizeof(type));
    allocTab[ret].typeListLast = types.at(t).lastOfType;
    if (types.at(t).firstOfType == NULL_ENTRY) types.at(t).firstOfType = ret;
    else allocTab[types.at(t).lastOfType].typeListNext = ret;
    types.at(t).lastOfType = ret;
    allocTab[ret].typeListNext = NULL_ENTRY;
    auto h = getHandlesOfType(t);
    if (h->init) h->init->call(ret, allocationMap);
    return ret;
  }

  template<size_t idx, class T, class U, class V, class... Zs>
  constexpr void Database::createPrecompiledBatch(precompiledBatch_t& out, U T::*u, V T::*v, Zs... z) {
    createPrecompiledBatch<idx>(out, u);//to specialization
    createPrecompiledBatch<idx+1>(out, v, std::forward<Zs>(z)...);//recurse or specilization
  }

  template<size_t idx, class T, class U>
  constexpr void Database::createPrecompiledBatch(precompiledBatch_t& out, U T::*u) {
    out.data[idx] = { (size_t)u, sizeof(U) };
  }

  template<class T, class... Zs>
  constexpr auto Database::createPrecompiledBatch(Zs... z) {
    size_t len = sizeof...(Zs);
    precompiledBatch_t ret = { std::make_unique<precompiledBatch_entry[]>(len), len };
    createPrecompiledBatch<len, 0, T, Zs...>(&ret, std::forward<Zs>(z)...);//populate
    return ret;
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

  class export_def Object {
  public:
	  Object() = default;
	  Object(const Object&) = delete;
	  ~Object() = default;
	  virtual Transform getTrans() = 0;
	  virtual void pushTrans(Transform*) = 0;
	  static Object* make(Database::Entry start, size_t transformOffset, WITE::Database::Entry* map);//
  };
  
}
