#pragma once

#include "Thread.h"
#include "RollingBuffer.h"

namespace WITE {

  struct strcmp_t {
    bool operator() (const std::string& a, const std::string& b) const { return a.compare(b) < 0; };
  };

  class export_def Database
    {
    public:
    typedef uint64_t Entry;
    typedef uint16_t type;
    typedef WITE::Callback_t<uint64_t, Entry, void*>* receiver_t;
    typedef Callback_t<void, Entry>* handle_t;
    typedef struct {
      handle_t update, init, destroy;
    } typeHandles;
    private:
    constexpr static size_t const& BLOCKSIZE = 4096;
    enum state_t { unallocated = 0, data, branch, trunk };//branches contain references to datas, trunks contain references to branches or other trunks
    enum logEntryType_t { del = 0, update };
    typedef struct {
      state_t state;
      type type;//global enum-esque maintained by consumer, used by getEntriesOfType. [0,127] reserved
    } _entry;
    public:
    constexpr static size_t const& BLOCKDATASIZE = BLOCKSIZE - sizeof(_entry);
    private:
    constexpr static size_t const& MAXPUTSIZE = BLOCKSIZE;//should be indexable by (enqueued)LogEntry::size
    constexpr static size_t const& MAXBLOCKPOINTERS = (BLOCKDATASIZE - sizeof(uint32_t)) / sizeof(Entry);//510
    typedef struct {
      uint32_t subblockCount;
      Entry subblocks[MAXBLOCKPOINTERS];
    } blocklistData;
    typedef struct {
      _entry header;
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
    } enqueuedLogEntry;
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
      cacheEntry* cacheLocation;//can be null
      uint64_t lastWrittenFrame, nextWriteFrame;
      size_t tlogHeadForObject, tlogTailForObject;//can be -1 for none
      state_t allocationState;//as of now, used for allocate/free, which might happen on the same frame but that's ok because they're managed by prime thread
      uint32_t readsThisFrame, readsLastFrame;//reset to 0 by master allocator, read to decide if it should be cached, dirty increment by other threads upon *disk* read
      Entry typeListNext, typeListLast;//linked list of all objects of this type, rebuilt on reload
    } allocationTableEntry;
    typedef struct {
      typedef RollingBuffer<enqueuedLogEntry, 65536, 0, uint16_t, &enqueuedLogEntry::size> transactionalBacklog_t;
      transactionalBacklog_t transactionalBacklog;
      uint64_t currentFileIdx;//target if it exists, active otherwise
      FILE *activeFile, *targetFile;
      volatile Entry allocRet = -1;
      volatile size_t allocSize = 0;
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
    //puts
    void put(Entry, const uint8_t * in, uint64_t start, uint16_t len);//out starts with the bit that is updated
    void put(Entry, const uint8_t * in, uint64_t* starts, uint64_t* lens, size_t count);//in starts at beginning of entry data
    template<class T> void put(Entry, const T * in);//everything, avoid this
    template<class T, class U> void put(Entry, const T * in, U T::*field);
    template<class T> void put(Entry, const T * in ...);
    //gets
    void get(Entry, uint8_t* out, uint64_t start, uint16_t len);//out starts with the bit that is fetched
    void get(Entry, uint8_t* out, uint64_t* starts, uint64_t* lens, size_t count);//out starts at beginning of entry data
    template<class T> void get(Entry, T* out);//everything, general use
    template<class T, class U> void get(Entry, T* out, U T::*field);
    template<class T> void get(Entry, T* out ...);
    Entry getChildEntryByIdx(Entry root, size_t idx);
    //inter object communication bs
    static void registerReceiver(const std::string, receiver_t);
    static void registerType(type t, typeHandles funcs);
    uint64_t sendMessage(const std::string, const Entry receiver, void* data);
    uint64_t sendMessageToAll(const std::string, const type receivers, void* data);
    type getEntryType(Entry e);
    static typeHandles* getHandlesOfType(type t);
    private:
    static const uint8_t zero[0];
    Database(const Database&) = delete;//no. just no.
    void push(enqueuedLogEntry*);
    Entry allocate(size_t size);
    void getRecurse(Entry, uint8_t* out, uint64_t* starts, uint64_t* lens, size_t count, size_t* offsetOfE, size_t* regionIdx);
    template<typename T> T getRaw(Entry e, size_t offset);//e contains offset
    void getRaw(Entry e, size_t offset, size_t size, uint8_t* out);//e contains offset, out starts at beginning of returned data
    //void getStateAtFrame(uint64_t frame, Entry, uint8_t* out, size_t len, size_t offset);//recurse starting at requested frame
    state_t getEntryState(Entry e) { return getRaw<state_t>(e, offsetof(loadedEntry, header.state)); }
    void getEntriesWithUpdate(std::vector<Entry>* out);
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
    static std::map<std::string, receiver_t, strcmp_t> messageReceivers;
    static std::vector<type> typesWithUpdates;//only edited on static init, pre multithread
    static std::map<type, perTypeStatic> typesStatic;
    std::map<type, perType> types;//on current frame
    RollingBuffer<logEntry, 0, 0, size_t, &logEntry::size, false> tlogManager;//TODO index of free?
    std::atomic_uint8_t masterThreadState;
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
    Entry pt_allocate(size_t size);
    bool pt_adoptTlog(threadResource_t::transactionalBacklog_t* src);
    bool pt_commissionTlog();
    bool pt_flushTlog(uint64_t minimumFreeSpace = 0);//commission from previous frames, until has specified free space (flush all if given 0)
    bool pt_mindSplitBase();
    void pt_rethinkLayout(size_t requiredNewEntries = 0);
    void pt_rethinkCache();
    void pt_push(logEntry*);
    };

  template<class T> Database::Entry Database::allocate(type t) {
    Entry ret = allocate(sizeof(T));
    put(ret, &t, offsetof(loadedEntry, header.type), sizeof(type));
    allocTab[ret].typeListLast = types[t].lastOfType;
    if (types[t].nextOfType == -1) types[t].nextOfType = ret;
    else allocTab[types[t].lastOfType].typeListNext = ret;
    types[t].lastOfType = ret;
    allocTab[ret].typeListNext = -1;
    return ret;
  }

  template<class T> void Database::put(Entry e, const T * in) {//everything, avoid this
    uint64_t size = sizeof(T), zero = 0;
    put(e, static_cast<uint8_t*>(in), &zero, &size, 1);
  }

  template<class T, class U> void Database::put(Entry e , const T * in, U T::*field) {
    uint64_t size = sizeof(U), offset = offsetof(T, *field);
    put(e, static_cast<uint8_t*>(in), &offset, &size, 1);
  }

  template<class T> void Database::put(Entry e, const T * in ...) {
    uint64_t starts[1024], lens[1024], i;
    T::* field;
    va_list fields;
    va_start(fields, in);
    field = va_arg(fields, T::*);
    do {
      for (i = 0;field && i < 1024;i++) {
	starts[i] = offsetof(T, *field);
	lens[i] = sizeof(*field);
	field = va_arg(fields, T::*);
      }
      put(e, reinterpret_cast<const uint8_t *>(in), starts, lens, i);
    } while (field);
    va_end(fields);
  }

  template<class T> void Database::get(Entry e, T* out) {
    get(e, reinterpret_cast<uint8_t*>(out), 0, sizeof(T));
  }

  template<class T, class U> void Database::get(Entry e, T* out, U T::*field) {
    get(e, reinterpret_cast<uin8_t*>(out), offsetof(T, *field), sizeof(U));
  }

  template<class T> void Database::get(Entry e, T* out ...) {
    uint64_t starts[1024], lens[1024], i;
    T::* field;
    va_list fields;
    va_start(fields, in);
    field = va_arg(fields, T::*);
    do {
      for (i = 0;field && i < 1024;i++) {
	starts[i] = offsetof(T, *field);
	lens[i] = sizeof(*field);
	field = va_arg(fields, T::*);
      }
      get(e, reinterpret_cast<uint8_t *>(in), starts, lens, i);
    } while (field);
    va_end(fields);
  }
  
}
