#pragma once

class Database : public WITE::Database {
public:
  Database(const char * filenamefmt, size_t cachesize, int64_t loadidx = -1);//filenamefmt must contain %d
  Database(size_t cachesize);//temp db, no file backing, for static content like the main menu or splash
  ~Database();
  void gracefulStop();
  void rebase();//advance index on current template
  void rebase(const char *, int64_t startidx);//save
  void free(Entry);
  void advanceFrame();//only called by master thread between renders
  size_t getEntriesOfType(type type, Entry* out, size_t maxout);//returns number of entries
  //puts
  void put(Entry, const uint8_t * in, uint64_t start, uint16_t len);//out starts with the bit that is updated
  void put(Entry, const uint8_t * in, uint64_t* starts, uint64_t* lens, size_t count);//in starts at beginning of entry data
  void put(Entry, const uint8_t * in, const precompiledBatch_t*);
  //gets
  void get(Entry, uint8_t* out, uint64_t start, uint16_t len);//out starts with the bit that is fetched
  void get(Entry, uint8_t* out, uint64_t* starts, uint64_t* lens, size_t count);//out starts at beginning of entry data
  // void get(Entry, uint8_t* out, const precompiledBatch_t*);//TODO//
  Entry getChildEntryByIdx(Entry root, size_t idx);
  //inter object communication bs
  //static void registerReceiver(const std::string, receiver_t);
  static inline void registerType(type t, typeHandles funcs);
  //uint64_t sendMessage(const std::string, const Entry receiver, void* data);
  //uint64_t sendMessageToAll(const std::string, const type receivers, void* data);
  type getEntryType(Entry e);
  static inline typeHandles* getHandlesOfType(type t);
  inline void getEntriesWithUpdate(std::vector<Entry>* out);//pulled by entry in update
  WITE::Object** getObjectInstanceFor(Entry e);
  uint64_t getCurrentFrame();
private:
  Database(const Database&) = delete;//no. just no.
  void push(enqueuedLogEntry*);
  Entry allocate(size_t size, type t);
  void getRecurse(Entry, uint8_t* out, uint64_t* starts, uint64_t* lens, size_t count, size_t* offsetOfE, size_t* regionIdx);
  template<typename T> T getRaw(Entry e, size_t offset);//e contains offset
  template<typename T> T getRaw(Entry e, size_t offset, uint64_t frame);//e contains offset
  void getRaw(Entry e, size_t offset, size_t size, uint8_t* out, uint64_t frame);//e contains offset, out starts at beginning of returned data
  state_t getEntryState(Entry e);
  state_t pt_getEntryState_live(Entry e);//pt only to prevent collision with tlogManager.relocate during pt_rethinkLayout
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
  std::map<type, perType> types;//on current frame
  static std::map<type, perTypeStatic> typesStatic;
  static std::vector<type> typesWithUpdates;//only edited on static init, pre multithread
  WITE::RollingBuffer<logEntry, size_t, 0, 0, offsetof(logEntry, size), false> tlogManager;//TODO index of free?
  std::atomic_uint8_t masterThreadState = 0;
  std::atomic_uint8_t dbStatus;
  WITE::ThreadResource<threadResource_t> threadResources;
  //prime thread only: (all transient)
  constexpr static size_t MAX_BATCH_FLUSH = 32;
  constexpr static size_t BUFFER_SIZE = 65536;
  size_t pt_starts[MAX_BATCH_FLUSH], pt_writenBytesThisFrame = 0, pt_writenBytesLastFrame = 0, pt_freeSpace = 0;
  uint64_t pt_lastWriteFrame;
  uint32_t pt_tid;
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
          //uint8_t data[1];
        };
      };
    } enqueuedLog;
    struct {
      logEntry header;
      uint8_t data[1];
    } log;
  } pt_buffer;
  struct {
    cacheIndex* pt_cacheStaging;
    size_t pt_cacheStagingSize, pt_cacheStagingLen;
    Entry entryScanIdx;
    size_t cacheScanIdx;
  } pt_rethinkCache_asyncVars;
  void primeThreadEntry();
  Entry pt_allocate(size_t size, Entry* map);
  bool pt_adoptTlog(threadResource_t::transactionalBacklog_t* src);
  bool pt_commitTlog();
  bool pt_flushTlog(uint64_t minimumFreeSpace = 0);//commit from previous frames, until has specified free space (flush all if given 0)
  bool pt_mindSplitBase();
  void pt_rethinkLayout(size_t requiredNewEntries = 0);
  void pt_rethinkCache();
  void pt_push(logEntry*);
  void pt_syncToDisk();
};
