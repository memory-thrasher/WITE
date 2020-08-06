#include <cstdio>
#include "stdafx.h"
#include "Database.h"
#include "Export.h"

#define BLOCK_INCREMENT_SIZE (512*1024*1024) //512mb
#define BLOCK_INCREMENT_COUNT (BLOCK_INCREMENT_SIZE/Database::BLOCKSIZE)
#define MIN_TLOG_CACHE_SIZE (sizeof(cacheEntry) * 16 + sizeof(logEntry) * 16 + 65536)

#define LOG_ENTRY_T_UPDATE 1
#define LOG_ENTRY_T_DELETE 2
#define LOG_ENTRY_T_CREATE 3

#define CACHE_SYNC_STATE_OUTBOUND_WRITE_PENDING 1
#define CACHE_SYNC_STATE_INBOUND_WRITE_PENDING 2

#define DB_STATUS_LOG_IN_THREAD_QUEUE 1

namespace WITE {

  std::vector<Database::type> Database::typesWithUpdates;
  std::map<Database::type, Database::perTypeStatic> Database::typesStatic;

  Database::Database(const char * filenamefmt, size_t cachesize, int64_t loadidx) : 
    filenamefmt(filenamefmt), filenameIdx(loadidx),
    allocTabRaw(malloc(cachesize)), primeRamSize(cachesize), pt_cacheStaging(NULL) {
    //TODO audit default allocation of prime ram
    size_t fileblocks, idealblocks;
    Entry i;
    _entry header;
    perType pt;
    char buffer[glm::max<size_t>(L_tmpnam, BLOCKSIZE)];
    FILE *active;
    for (auto t = typesStatic.begin();t != typesStatic.end();t++)
	    types[t->first] = { NULL_ENTRY, NULL_ENTRY };
    memset(allocTabRaw, 0, cachesize);
    currentFrame = targetFrame = 0;
    if(!filenamefmt) {
      std::tmpnam(buffer);//make temp file name
    } else {
      sprintf(buffer, filenamefmt, loadidx);
    }
    this->active = buffer;
    fileblocks = getFileSize(buffer) / BLOCKSIZE;
    if (MIN_TLOG_CACHE_SIZE > cachesize - (int64_t)fileblocks * sizeof(allocationTableEntry))
      CRASH("cache too small for allocation table");
    active = fopen(buffer, "a+b");
    if (!active) CRASH("Failed to open database file\n");
    fseek(active, fileblocks * BLOCKSIZE, SEEK_SET);
    memset(buffer, 0, BLOCKSIZE);
    atCount = fileblocks;
    tlogSize = primeRamSize - sizeof(allocationTableEntry) * atCount;
    idealblocks = tlogSize / (3 * sizeof(cacheEntry));
    while (fileblocks < idealblocks) {
      fwrite(buffer, BLOCKSIZE, 1, active);
      fileblocks++;
    }
    atCount = fileblocks;
    tlogSize = primeRamSize - sizeof(allocationTableEntry) * atCount;
    idealblocks = cacheCount = tlogSize / (3 * sizeof(cacheEntry));
    tlogSize -= cacheCount * sizeof(cacheEntry);
    cacheRaw = allocTabByte + primeRamSize - cacheCount * sizeof(cacheEntry);
    fseek(active, 0, SEEK_SET);
    for (i = 0;i < fileblocks;i++) {
      if (idealblocks >= fileblocks) {
	fread(&cache[i].e, sizeof(cache[i].e), 1, active);
	header = cache[i].e.header;
	cache[i].lastUseFrame = cache[i].syncstate = cache[i].readsCachedLifetime = 0;
	allocTab[i].cacheLocation = &cache[i];
      } else {
	fseek(active, (size_t)(i * sizeof(loadedEntry)), SEEK_SET);
	fread(&header, sizeof(header), 1, active);
	allocTab[i].cacheLocation = NULL;
      }
      allocTab[i].lastWrittenFrame = allocTab[i].readsThisFrame = 0;
      allocTab[i].tlogHeadForObject = allocTab[i].tlogTailForObject = NULL_OFFSET;
      allocTab[i].allocationState = header.state;
      pt = types[header.type];
      if (!pt.firstOfType) {
	pt.firstOfType = i;
	allocTab[i].typeListLast = NULL_ENTRY;
      } else {
	allocTab[pt.lastOfType].typeListNext = i;
	allocTab[i].typeListLast = pt.lastOfType;
      }
      allocTab[i].typeListNext = NULL_ENTRY;
      pt.lastOfType = i;
      types[header.type] = pt;
    }
    fclose(active);
    tlogManager.relocate(allocTabByte + atCount * sizeof(allocationTableEntry), tlogSize);
    pt_cacheStaging = new cacheIndex[cacheCount];
    pt_cacheStagingSize = cacheCount;
    Thread::spawnThread(WITE::CallbackFactory<void>::make<Database>(this, &Database::primeThreadEntry));
  }

  Database::Database(size_t cachesize) : Database(NULL, cachesize) {}

  Database::~Database() {
    masterThreadState = 2;
    while (masterThreadState != 3);
  }

  void Database::rebase() {
    char filename[4096];
    if (target[0]) {
      LOG("WARN: database: rebase when previous rebase has not finished, skipping.\n");
      return;
    }
    filenameIdx++;
    sprintf(filename, filenamefmt, filenameIdx);
    target = filename;
    targetFrame = currentFrame;
    fileIdx++;
  }

  void Database::rebase(const char * name, int64_t startidx) {
    filenameIdx = startidx - 1;
    filenamefmt = name;
    rebase();
  }

  void Database::push(enqueuedLogEntry* ele) {
    threadResources.get()->transactionalBacklog.pushBlocking(ele);
    dbStatus.store(DB_STATUS_LOG_IN_THREAD_QUEUE, std::memory_order_release);
  }

  void Database::free(Entry d) {
    enqueuedLogEntry ele = { currentFrame, del, 0, 0, d };
    push(&ele);
  }

  void Database::advanceFrame() {
    currentFrame++;
    //TODO metadata flush? (or else handle sync/frame issues on entry type iterator)
    while(dbStatus.load(std::memory_order_consume) & DB_STATUS_LOG_IN_THREAD_QUEUE) {}
  }

  size_t Database::getEntriesOfType(type t, Entry* out, size_t maxout) {//may be stale
    Entry next = types[t].firstOfType;
    size_t count = 0;
    type fromEntry;
    while (next && count < maxout) {
      fromEntry = getEntryType(next);
      if (t == fromEntry) {
	out[count] = next;
	count++;
      }
      next = allocTab[next].typeListNext;
    }
    return count;
  }

  void Database::put(Entry target, const uint8_t * in, uint64_t start, uint16_t len) {
    struct {
      enqueuedLogEntry ele;
      uint8_t data[Database::MAXPUTSIZE];//keep these in order
    } loaded;
    start += offsetof(loadedEntry, data);
    loaded.ele = {this->currentFrame, update, len, start, target};
    memcpy(static_cast<void*>(loaded.data), in, len);
    push(&loaded.ele);
  }

  void Database::put(Entry e, const uint8_t * inRaw, uint64_t* starts, uint64_t* lens, size_t count) {
    //in starts at beginning of entry data
    constexpr static uint16_t MAX_GAP = sizeof(logEntry) + 64;
    //save overhead space if flushing many closely clustered tiny pieces
    //TUNEME: how much ram is worth one fewer log transactions to flush?
    uint64_t start, end, i;
    uint64_t len;
    for (i = 0;i < count;i++) {
      start = starts[i];
      len = lens[i];
      end = start + len;
      while (i < count - 1 && starts[i + 1] <= end + MAX_GAP &&
	     starts[i + 1] + lens[i + 1] <= start + Database::BLOCKDATASIZE) {//compress multi-write
	i++;
	end = starts[i] + lens[i];
	len = end - start;
      }
      put(e, inRaw + start, start, (uint16_t)len);
    }
  }

  void Database::put(Entry t, const uint8_t * inRaw, const precompiledBatch_t* fields) {
    constexpr static uint16_t MAX_GAP = sizeof(logEntry) + 64;
    uint64_t start, end, i;
    uint64_t len;
    for (i = 0;i < fields->size;i++) {
      start = fields->data[i].start;
      len = fields->data[i].len;
      end = start + len;
      while (i < fields->size - 1 && fields->data[i+1].start <= end + MAX_GAP &&
	     fields->data[i+1].start + fields->data[i + 1].len <= start + Database::BLOCKDATASIZE) {//TODO move this to precompile
	i++;
	end = fields->data[i].start + fields->data[i].len;
	len = end - start;
      }
      put(t, inRaw + start, start, (uint16_t)len);
    }
  }

  void Database::get(Entry e, uint8_t* out, uint64_t start, uint16_t len) {
    uint64_t len64 = len;
    get(e, out - start, &start, &len64, 1ul);
  }

  void Database::get(Entry e, uint8_t* out, uint64_t* starts, uint64_t* lens, size_t count) {
    size_t offsetOfE = 0, regionIdx = 0;
    getRecurse(e, out, starts, lens, count, &offsetOfE, &regionIdx);
  }

  void Database::getRecurse(Entry e, uint8_t* out, uint64_t* starts, uint64_t* lens, size_t count,
			    size_t* offsetOfE, size_t* regionIdx) {//TODO abstract the traversal of trunks and branches, so target entry (id'd by offset) can be cached
    size_t i, nextBlockIdx, offsetOfChild, start, end = 0;
    loadedEntry le;
    le.header.state = getEntryState(e);
    switch (le.header.state) {
    default: LOG("Warn: invalid state of block linked in compound tree. Something is wrong."); break;
    case unallocated: LOG("Warn: unallocated block linked in compound tree. Something is wrong."); break;
    case data:
      if(starts[*regionIdx] <= *offsetOfE + BLOCKDATASIZE) {
	//NOTE may be partial; region may span many Entries; select appropriate sub region
	start = starts[*regionIdx] <= *offsetOfE ? 0 : starts[*regionIdx] - *offsetOfE;
	while(*regionIdx < count && starts[*regionIdx] <= *offsetOfE + BLOCKDATASIZE && end < BLOCKDATASIZE) {
	  end = starts[*regionIdx] + lens[*regionIdx] - *offsetOfE;
	  if (end > BLOCKDATASIZE)
	    end = BLOCKDATASIZE;
	  else
	    (*regionIdx)++;
	}
	getRaw(e, start + offsetof(loadedEntry, data), end - start, out + *offsetOfE + start, currentFrame - 1);
      }
      break;
    case branch:
      le.children.subblockCount = getRaw<uint32_t>(e, offsetof(loadedEntry, children.subblockCount));
      nextBlockIdx = (starts[*regionIdx] - *offsetOfE) / BLOCKDATASIZE;
      while (*regionIdx < count && nextBlockIdx < le.children.subblockCount) {
	offsetOfChild = *offsetOfE + nextBlockIdx * BLOCKDATASIZE;
	getRecurse(getRaw<Entry>(e, __offsetof_array(loadedEntry, children.subblocks, nextBlockIdx)),
		   out, starts, lens, count, &offsetOfChild, regionIdx);
	nextBlockIdx = min(nextBlockIdx+1, (starts[*regionIdx] - *offsetOfE) / BLOCKDATASIZE);
      }
      *offsetOfE += le.children.subblockCount * BLOCKDATASIZE;
      break;
    case trunk://trunk and branch should be quick reads as they shouldn't have any tlogs after creation.
      le.children.subblockCount = getRaw<uint32_t>(e, offsetof(loadedEntry, children.subblockCount));
      getRaw(e, offsetof(loadedEntry, children.subblocks),
	     sizeof(le.children.subblocks[0]) * le.children.subblockCount,
	     reinterpret_cast<uint8_t*>(le.children.subblocks), currentFrame - 1);
      for (i = 0;*regionIdx < count && i < le.children.subblockCount;i++)
	getRecurse(le.children.subblocks[i], out, starts, lens, count, offsetOfE, regionIdx);
    }
  }

  void Database::getRaw(Entry e, size_t offset, size_t size, uint8_t* out, uint64_t frame) {
    cacheEntry* cache = allocTab[e].cacheLocation;
    FILE* active, *target;
    size_t tlogPoint, start, end;
    union {
      logEntry log;
      uint8_t rawLog[1];
    };
    if(currentFrame < frame) {
      //only read previous frames, so no reads on first frame
      //??memset(out, 0, size);
      CRASH("Attempted read the future. Shouldn't happen.");
    }
    if (cache) {
      memcpy(out, &cache->e.raw[offset], size);
      cache->lastUseFrame = currentFrame;
      cache->readsCachedLifetime++;
    } else {
      auto threadResource = threadResources.get();
      active = threadResource->activeFile;
      target = threadResource->targetFile;
      if (threadResource->currentFileIdx != fileIdx) {
	if (active) fclose(active);
	if (threadResource->currentFileIdx != fileIdx - 1) {
	  if (target) fclose(target);
	  active = NULL;
	} else
	  active = target;
	if(this->target[0])
          target = fopen(&this->target.front(), "rb");
	threadResource->currentFileIdx = fileIdx;
      }
      if(!active) active = threadResource->activeFile = fopen(&this->active.front(), "rb");
      threadResource->activeFile = active;
      threadResource->targetFile = target;
      CRASHIFWITHERRNO(fseek(active, offset + e * BLOCKSIZE, SEEK_SET));
      CRASHIFWITHERRNO(fread(out, size, 1, active) != 1);
      allocTab[e].readsThisFrame++;
    }
    if (allocTab[e].nextWriteFrame <= frame) {
      tlogPoint = allocTab[e].tlogHeadForObject;
      while (tlogPoint != NULL_ENTRY) {
	tlogManager.readFromHandle(rawLog, tlogPoint, sizeof(logEntry));
	if (log.start != e) CRASH("tlog fault; crashing to minimize save corruption.\n");
	if (log.frameIdx > frame) break;
	switch (log.type) {
	default: CRASH("tlog illegal type; crashing to minimize save corruption.\n");
	case del: memset(out, 0, size); break;
	case update:
	  start = max(offset, log.offset);
	  end = min(offset + size, log.offset + log.size);//FIXME log.offset is wrong
          if(start < end)
            tlogManager.readFromHandle(out + start - offset, tlogPoint + sizeof(logEntry), end - start);
	}
	tlogPoint = log.nextForObject;
      }
    }
  }

  template<typename T> T Database::getRaw(Entry e, size_t offset) {
    return getRaw<T>(e, offset, currentFrame - 1);
  }

  template<typename T> T Database::getRaw(Entry e, size_t offset, uint64_t frame) {
    union {
      T ret;
      uint8_t raw[0];
    };
    getRaw(e, offset, sizeof(T), raw, frame);
    return ret;
  }

  Database::state_t Database::getEntryState(Entry e) {
    return getRaw<state_t>(e, offsetof(loadedEntry, header.state), currentFrame-1);
  }

  Database::state_t Database::pt_getEntryState_live(Entry e) {
    return getRaw<state_t>(e, offsetof(loadedEntry, header.state), currentFrame);
  }

  Database::Entry Database::getChildEntryByIdx(const Entry root, const size_t idx) {
    size_t leafAbs = idx, leafRel = leafAbs % MAXBLOCKPOINTERS,
      branchAbs = leafAbs / MAXBLOCKPOINTERS, branchRel = branchAbs % MAXBLOCKPOINTERS,
      trunkAbs = branchAbs / MAXBLOCKPOINTERS;
    Entry current = root;
    while (trunkAbs && getEntryState(current) == trunk) {
      current = getRaw<Entry>(current, offsetof(blocklistData, subblocks[MAXBLOCKPOINTERS - 1]));
      trunkAbs--;
    }
    if(getEntryState(current) == trunk)
      current = getRaw<Entry>(current, offsetof(blocklistData, subblocks) + sizeof(Entry) * (trunkAbs ? MAXBLOCKPOINTERS - 1 : branchRel));
    if(getEntryState(current) == branch)
      current = getRaw<Entry>(current, offsetof(blocklistData, subblocks) + sizeof(Entry) * leafRel);
    return current;
  }

  Database::Entry Database::allocate(size_t size, Entry* map) {//allocation is handled by master thread, submit request and wait
    auto tr = threadResources.get();
    tr->map = map;
    tr->allocRet = NULL_ENTRY;
    tr->allocSize = size;
    dbStatus.store(DB_STATUS_LOG_IN_THREAD_QUEUE, std::memory_order_release);
    while (tr->allocRet == NULL_ENTRY) sleep(50);//TUNEME sleep time
    return tr->allocRet;
  }

  // void Database::registerReceiver(const std::string nom, receiver_t r) {
  //   messageReceivers[nom] = r;
  // }

  void Database::registerType(type t, typeHandles funcs) {
    typesStatic[t] = funcs;
    if (funcs.update) typesWithUpdates.push_back(t);
  }

  Database::type Database::getEntryType(Entry e) {
    return getRaw<type>(e, offsetof(loadedEntry, header.type));
  }

  Database::typeHandles* Database::getHandlesOfType(type t) {
    return &typesStatic[t];
  }

  void Database::getEntriesWithUpdate(std::vector<Entry>* out) {
    type i, t, typeCount;
    Entry selected;
    typeCount = typesWithUpdates.size();
    for(i = 0;i < typeCount;i++) {
      t = typesWithUpdates[i];
      selected = types[t].firstOfType;
      while(selected != NULL_ENTRY) {
	    out->push_back(selected);
	    selected = allocTab[selected].typeListNext;
      }
    }
  }

  Object** Database::getObjectInstanceFor(Entry e) {
    return &allocTab[e].obj;
  }

  // uint64_t Database::sendMessage(const std::string message, const Entry receiver, void* data) {
  //   messageReceivers[message]->call(receiver, data);
  // }

  // uint64_t Database::sendMessageToAll(const std::string message, const type receivers, void* data) {
  //   Entry next = types[receivers].firstOfType;
  //   while (next != -1) {
  //     sendMessage(message, next, data);
  //     next = allocTab[next].typeListNext;
  //   }
  // }

  /*
    notes to db prime thread:
    create/delete updates per type linked list
    split Entry going into master tlog: enqueuedTLog has root Entry for multi-entry objects,
    but get() needs tlog linked based on child Entry
  */
  void Database::primeThreadEntry() {//TODO start over, use shild functions
    size_t i, count, j = 0;
    bool didSomething;
    decltype(threadResources)::Tentry* trs;
    if (masterThreadState.exchange(1) != 0) CRASH("Attempted to launch second master db thread.\n");
    pt_activeF = fopen(&active.front(), "a+b");
  do_something:
    while (masterThreadState == 1) {
      if (target[0] && !pt_targetF)
	pt_targetF = fopen(&target.front(), "rb");
      didSomething = false;
      count = threadResources.listAll(&trs);
      for (i = 0;i < count;i++)
	if (trs[i] && trs[i]->allocSize) {
	  trs[i]->allocRet = pt_allocate(trs[i]->allocSize, trs[i]->map);
	  trs[i]->allocSize = 0;
	  didSomething = true;
	}
      if (didSomething) goto do_something;
      for (i = 0;i < count;i++)
	if (trs[i] && trs[i]->transactionalBacklog.used())
	  didSomething |= pt_adoptTlog(&trs[i]->transactionalBacklog);
      if (didSomething) goto do_something;
      dbStatus &= ~DB_STATUS_LOG_IN_THREAD_QUEUE;
      if (pt_commitTlog()) goto do_something;
      TIME(didSomething = pt_mindSplitBase(), 2, "\tdb: mind split base: %llu\n");
      if (didSomething) goto do_something;
      switch (j++) {
      case 0: TIME(pt_rethinkLayout(), 2, "\tdb:rethink layout: %llu\n"); break;
      case 1: TIME(pt_rethinkCache(), 2, "\tdb:rethink cache: %llu\n"); break;
      default: j = 0; break;
      }
    }
    while(pt_mindSplitBase());
    while(pt_commitTlog());
    masterThreadState = 3;
  }

  void Database::pt_rethinkLayout(size_t requiredNewEntries) {
    size_t i, idealFree, newRecordCount, newCacheCount, newTlogSize;
    union { size_t j; uint8_t jRaw[1]; };
    cacheEntry *ce, *newCache;
    logEntry* le;
    pt_flushTlog();//??
    idealFree = atCount * 2 / 10 + requiredNewEntries;
    newRecordCount = min(idealFree + atCount - pt_freeSpace, atCount);
    newCacheCount = (primeRamSize - newRecordCount * sizeof(allocationTableEntry) -
		     max(pt_writenBytesLastFrame, pt_writenBytesThisFrame, 65536)) / (2 * sizeof(cacheEntry));
    newTlogSize = primeRamSize - sizeof(cacheEntry) * newCacheCount - sizeof(allocationTableEntry) * newRecordCount;
    if (newTlogSize > primeRamSize || newCacheCount > primeRamSize / sizeof(cacheEntry) || newTlogSize > primeRamSize)
      CRASH("Out of prime ram\n");
    CRASHIFFAIL(tlogManager.relocate(allocTabByte + newRecordCount * sizeof(allocationTableEntry), newTlogSize));
    newCache = cache + newCacheCount - cacheCount;
    if(newCacheCount < cacheCount) {//flushs some things
      for(ce = cache;std::less<cacheEntry*>()(ce, newCache);ce++) {
	i = ce->entry;
	if (ce->syncstate & CACHE_SYNC_STATE_OUTBOUND_WRITE_PENDING) {
	  ce->syncstate &= ~CACHE_SYNC_STATE_OUTBOUND_WRITE_PENDING;
	  FILE* dst = pt_targetF && allocTab[ce->entry].lastWrittenFrame >= targetFrame ? pt_targetF : pt_activeF;
	  fseek(dst, sizeof(loadedEntry) * i, SEEK_SET);
	  fwrite(static_cast<void*>(&ce->e), sizeof(loadedEntry), 1, dst);
	}
	allocTab[i].cacheLocation = NULL;
      }
    }
    if (newRecordCount > atCount) {
      memset(allocTab + atCount, 0, (newRecordCount - atCount) * sizeof(allocationTableEntry));
      FILE* dst = pt_targetF ? pt_targetF : pt_activeF;
      fseek(dst, sizeof(loadedEntry) * atCount, SEEK_SET);
      for (;atCount < newRecordCount;atCount++)
	fwrite(zero, sizeof(zero), 1, dst);
    }
  }

  void Database::pt_rethinkCache() {//called when prime thread has time to burn, but don't take too long; return to yield
    //TODO yield more, this takes way too long
    Entry i;
    size_t j, len, score, top, bot, mid;
    allocationTableEntry ate;
    cacheEntry ce;
    cacheIndex ci;
    if (pt_flushTlog()) return;//not now, something happened
    if (pt_cacheStagingSize < cacheCount) {
      pt_cacheStagingSize = cacheCount + (cacheCount >> 3);//+12.5% margin
      delete[] pt_cacheStaging;
      pt_cacheStaging = new cacheIndex[pt_cacheStagingSize];
      return;
    }
    memset(pt_cacheStaging, 0, sizeof(cacheIndex) * pt_cacheStagingSize);
    for (i = len = 0;i < atCount;i++) {
      ate = allocTab[i];//faster ram read all at once because allocTab is volatile
      if(ate.allocationState == unallocated)
        score = 1;
      else
        score = max(ate.readsLastFrame, ate.readsThisFrame) +
	  (ate.cacheLocation ? 48>>(currentFrame - ate.cacheLocation->readsCachedLifetime) : 0) +
	  (ate.nextWriteFrame != NULL_FRAME ? (ate.cacheLocation ? 512 : 16) :
	   ate.lastWrittenFrame != NULL_FRAME ? 48 >> (currentFrame - ate.lastWrittenFrame) : 0);
      if (len) {
        if(cacheCount == len && score <= pt_cacheStaging[len - 1].score) continue;
	bot = 0;
	top = len - 1;
	mid = (bot + top) >> 1;
	while (bot != top && pt_cacheStaging[mid].score != score) {
	  if (score < pt_cacheStaging[mid].score) bot = mid;
	  else top = mid;
	  mid = (bot + top) >> 1;
	}
      } else mid = 0;
      if (mid < atCount) {
	if (len < atCount) {//insert, grow list
	  if (mid < len) memmove(&pt_cacheStaging[mid + 1], &pt_cacheStaging[mid], sizeof(cacheIndex) * (len - mid));
	  len++;
	} else {//insert, drop lowest
	  if (mid < len - 1) memmove(&pt_cacheStaging[mid + 1], &pt_cacheStaging[mid], sizeof(cacheIndex) * (len - mid - 1));
	}
	pt_cacheStaging[mid].score = score;
	pt_cacheStaging[mid].ent = ate.allocationState == unallocated ? NULL_ENTRY : i;
      }
    }
    for (j = 0;j < cacheCount;j++) {
      ce = cache[j];
      ci = pt_cacheStaging[j];
      if(ci.ent == NULL_ENTRY) continue;
      if (ce.entry != ci.ent && ce.entry != NULL_ENTRY && allocTab[ce.entry].cacheLocation == &cache[j]) {//cache slot occupied
	if (ce.syncstate & CACHE_SYNC_STATE_OUTBOUND_WRITE_PENDING) {//write before overwriting
	  FILE* dst = pt_targetF && allocTab[ce.entry].lastWrittenFrame >= targetFrame ? pt_targetF : pt_activeF;
	  fseek(dst, (long)(ce.entry * sizeof(loadedEntry)), SEEK_SET);
	  fwrite(&ce.e, sizeof(loadedEntry), 1, dst);
	}
	allocTab[ce.entry].cacheLocation = NULL;
      }
      if (allocTab[ci.ent].cacheLocation) {
	if (allocTab[ci.ent].cacheLocation == &cache[j]) continue;//already cached here
	memcpy(static_cast<void*>(&cache[j]), static_cast<void*>(allocTab[ci.ent].cacheLocation), sizeof(cacheEntry));
      } else {//read
	FILE* src = pt_targetF && allocTab[ci.ent].lastWrittenFrame >= targetFrame ? pt_targetF : pt_activeF;
	fseek(src, (long)(ci.ent * sizeof(loadedEntry)), SEEK_SET);
	fread(&ce.e, sizeof(loadedEntry), 1, src);
	ce.lastUseFrame = currentFrame;
	ce.entry = ci.ent;
	ce.syncstate = 0;
	ce.readsCachedLifetime = 0;
	cache[j] = ce;//ce is on thread stack, which is cached, while cache is not cached
      }
      allocTab[ci.ent].cacheLocation = &cache[j];
    }
  }

#define PUSH_LE pt_push(le)
  //TODO skip flush/rethink if enough space
  void Database::pt_push(logEntry* le) {
    size_t handle;
    pt_flushTlog(le->size + sizeof(logEntry));
    CRASHIFFAIL(tlogManager.push(le, &handle));
    if(allocTab[le->start].tlogTailForObject != allocationTableEntry::OBJECT_NULL)
      tlogManager.writeFromHandle(allocTab[le->start].tlogTailForObject + offsetof(logEntry, nextForObject), reinterpret_cast<uint8_t*>(&handle), sizeof(handle));
    allocTab[le->start].tlogTailForObject = handle;
    if(allocTab[le->start].tlogHeadForObject == allocationTableEntry::OBJECT_NULL)
      allocTab[le->start].tlogHeadForObject = handle;
    if (le->type == logEntryType_t::del) {
      allocTab[le->start].allocationState = state_t::unallocated;
      pt_freeSpace++;
    }
    if(pt_lastWriteFrame != currentFrame) {
      pt_writenBytesLastFrame = pt_writenBytesThisFrame;
      pt_writenBytesThisFrame = 0;
      pt_lastWriteFrame = currentFrame;
    }
    pt_writenBytesThisFrame += le->size;
    if(allocTab[le->start].nextWriteFrame == NULL_FRAME) allocTab[le->start].nextWriteFrame = currentFrame;
    allocTab[le->start].lastWrittenFrame = currentFrame;
  }

  Database::Entry Database::pt_allocate(size_t size, Entry* map) {//evergreen model
    size_t numDataEntries = (size - 1) / BLOCKDATASIZE + 1,
      numBranches = numDataEntries <= 1 ? 0 : (numDataEntries - 1) / MAXBLOCKPOINTERS + 1,
      numTrunks = numBranches <= 1 ? 0 : (numBranches - 2) / (MAXBLOCKPOINTERS - 1) + 1,
      numBlocks = numDataEntries + numBranches + numTrunks, i, subblockCount, orphans = numDataEntries,
      unboundBranches = numTrunks;
    static Entry entries[MAXBLOCKPOINTERS*MAXBLOCKPOINTERS];//one trunk per loop
    static struct {
      logEntry le;
      loadedEntry data;
    } statePutter;
    logEntry* le = &statePutter.le;
    Entry j;
    if (numBlocks > sizeof(entries) / sizeof(entries[0])) { LOG("NYI object size requested too big."); CRASHRET(0); }//TODO loop with same array
    for (i = j = 0;i < numBlocks;i++) {
      while (allocTab[j].allocationState) {
	j++;
	if (j == atCount)//out of space (not enough unallocated entries)
	  pt_rethinkLayout(numBlocks - i);//gauranteed allocation or crash, so no fail case to check
      }
      pt_freeSpace--;
      entries[i] = j;
      if (i < numDataEntries) {
	allocTab[j].allocationState = data;
        statePutter.data.header.state = data;
	*le = { currentFrame, update, sizeof(state_t), offsetof(loadedEntry, header.state), allocationTableEntry::OBJECT_NULL, j };
	PUSH_LE;
      } else if(i < numBranches + numDataEntries) {
	allocTab[j].allocationState = branch;
	statePutter.data.children.subblockCount = subblockCount = min(orphans, MAXBLOCKPOINTERS);
	memcpy(statePutter.data.children.subblocks, &entries[numDataEntries - orphans], subblockCount * sizeof(Entry));
	orphans -= subblockCount;
	statePutter.data.header = { branch, 0 };
	*le = { currentFrame, update, sizeof(_entry) + sizeof(blocklistData::subblockCount) + subblockCount * sizeof(Entry), 0, allocationTableEntry::OBJECT_NULL, j };
	PUSH_LE;
      } else {
	allocTab[j].allocationState = trunk;
	statePutter.data.children.subblockCount = subblockCount = min(unboundBranches, MAXBLOCKPOINTERS - !(i == numBlocks-1));
	memcpy(statePutter.data.children.subblocks, &entries[numBranches + numDataEntries - unboundBranches],
	       subblockCount * sizeof(Entry));
	unboundBranches -= subblockCount;
	statePutter.data.header = { trunk, 0 };
	*le = { currentFrame, update, sizeof(_entry) + sizeof(blocklistData::subblockCount) + subblockCount * sizeof(Entry), 0, allocationTableEntry::OBJECT_NULL, j };
	PUSH_LE;
      }
    }
    if(map) memcpy(map, entries, min(ALLOCATION_MAP_SIZE, numDataEntries * sizeof(entries[0])));
    j = entries[0];
    return j;
  }

  bool Database::pt_adoptTlog(threadResource_t::transactionalBacklog_t* src) {
    size_t discard, read = 0, targetChild, offset, size, trunkIdx, branchIdx, targetTrunk, targetBranch, targetLeaf;
    Entry trunk, branch, leaf;
    state_t baseState;
    union {
      logEntry* le;
      uint8_t* leRaw;
    };
    enqueuedLogEntry ele;
    le = &pt_buffer.log.header;
    if (src->pop(pt_buffer.enqueuedLog.raw, &discard, sizeof(pt_buffer.raw), 1) == RB_BUFFER_UNDERFLOW) return false;
    ele = pt_buffer.enqueuedLog.header;
    if (ele.type == del) {
      while (pt_getEntryState_live(trunk) == Database::state_t::trunk) {
	for (branchIdx = 0;branchIdx < MAXBLOCKPOINTERS;branchIdx++) {
	  branch = getRaw<Entry>(trunk, __offsetof_array(loadedEntry, children, branchIdx), currentFrame);
	  if (pt_getEntryState_live(branch) == Database::state_t::trunk) break;
	  for (targetChild = 0;targetChild < MAXBLOCKPOINTERS;targetChild++) {
	    leaf = getRaw<Entry>(branch, __offsetof_array(loadedEntry, children, targetChild), currentFrame);
	    if (!leaf) break;
	    *le = { ele.frameIdx, del, 0, 0, allocationTableEntry::OBJECT_NULL, leaf };
	    PUSH_LE;
	  }
	  *le = { ele.frameIdx, del, 0, 0, allocationTableEntry::OBJECT_NULL, branch };
	  PUSH_LE;
	}
	*le = { ele.frameIdx, del, 0, 0, allocationTableEntry::OBJECT_NULL, trunk };
	PUSH_LE;
	trunk = branch;
      }
      if(pt_getEntryState_live(trunk) == Database::state_t::branch) {
	branch = trunk;
	for (targetChild = 0;targetChild < MAXBLOCKPOINTERS;targetChild++) {
	  leaf = getRaw<Entry>(branch, __offsetof_array(loadedEntry, children.subblocks, targetChild), currentFrame);
	  if (!leaf) break;
	  *le = { ele.frameIdx, del, 0, 0, allocationTableEntry::OBJECT_NULL, leaf };
	  PUSH_LE;
	}
	*le = { ele.frameIdx, del, 0, 0, allocationTableEntry::OBJECT_NULL, branch };
	PUSH_LE;
      }
    } else {
      offset = ele.offset;
      trunkIdx = 0;
      branchIdx = NULL_OFFSET;
      leaf = branch = trunk = ele.start;
      baseState = pt_getEntryState_live(ele.start);
      do {
	targetChild = offset / BLOCKSIZE;
	targetLeaf = targetChild % MAXBLOCKPOINTERS;
	targetBranch = targetChild / MAXBLOCKPOINTERS;
	targetTrunk = targetBranch / (MAXBLOCKPOINTERS - 1);
        switch(baseState) {//possibly the only valid use case for case passthrough to ever exist
        case state_t::trunk:
          while(trunkIdx < targetTrunk) {
            trunk = getRaw<Entry>(trunk, offsetof(loadedEntry, children.subblocks[MAXBLOCKPOINTERS - 1]), currentFrame);
            trunkIdx++;
            if(trunkIdx == targetTrunk - 1 && pt_getEntryState_live(trunk) == Database::state_t::branch) targetTrunk--;
          }
          branchIdx = trunkIdx * MAXBLOCKPOINTERS;
          branch = getRaw<Entry>(trunk, __offsetof_array(loadedEntry, children.subblocks, targetBranch - branchIdx), currentFrame);
        case state_t::branch:
          leaf = getRaw<Entry>(branch, __offsetof_array(loadedEntry, children.subblocks, targetLeaf - branchIdx * MAXBLOCKPOINTERS), currentFrame);
        case state_t::data:
          size = min(ele.size + ele.offset - offset, BLOCKSIZE);
          *le = {ele.frameIdx, update, size, offset % BLOCKSIZE, allocationTableEntry::OBJECT_NULL, leaf};
          offset += size;
          read += size;
          PUSH_LE;
          leRaw += size + sizeof(logEntry);
          continue;
        default:
          CRASHRET("Unrecognized state: dirty read", true);
        }
      } while (read < ele.size);
    }
    return true;
  }

#undef PUSH_LE

  bool Database::pt_commitTlog() {
    uint64_t maxframe = pt_targetF ? targetFrame : currentFrame;
    size_t i = 0;
    logEntry *le = &pt_buffer.log.header;
    cacheEntry* ce;
    if (tlogManager.peek(pt_buffer.raw, pt_starts, BUFFER_SIZE, MAX_BATCH_FLUSH, NULL) == RB_BUFFER_UNDERFLOW)
      return false;
    do {
      if (le->frameIdx >= maxframe) break;
      ce = allocTab[le->start].cacheLocation;
      if (ce) {
	switch (le->type) {
	case del: memset(static_cast<void*>(&ce->e), 0, sizeof(loadedEntry)); break;
	case update: memcpy((void*)((uint8_t*)&ce->e + le->offset), (void*)((uint8_t*)le+sizeof(logEntry)), le->size); break;
	}
	ce->lastUseFrame = currentFrame;
	ce->syncstate |= CACHE_SYNC_STATE_OUTBOUND_WRITE_PENDING;
	//TODO clear inbound write flag?
      } else {
	fseek(pt_activeF, sizeof(loadedEntry) * le->start + le->offset, SEEK_SET);
	switch (le->type) {
	case del: fwrite(zero, sizeof(loadedEntry), 1, pt_activeF); break;
	case update: fwrite(static_cast<void*>(le + 1), le->size, 1, pt_activeF); break;
	}
      }
      //TODO allocTab nextWriteFrame? (what is it for?)
      allocTab[le->start].lastWrittenFrame = le->frameIdx;
      allocTab[le->start].tlogHeadForObject = le->nextForObject;
      if (le->nextForObject == NULL_OFFSET) allocTab[le->start].tlogTailForObject = NULL_OFFSET;
      le = reinterpret_cast<logEntry*>(pt_buffer.raw + pt_starts[i]);
      i++;
      tlogManager.drop();
    } while (pt_starts[i]);
    return i;
  }

  bool Database::pt_flushTlog(uint64_t minFree) {
    bool ret;
    if (minFree && tlogManager.freeSpace() >= minFree) return false;
    if (minFree) {
      while (tlogManager.freeSpace() < minFree)
	if (!pt_commitTlog())
	  CRASHRET(false);
      return true;
    } else {
      ret = pt_commitTlog();
      while (pt_commitTlog());
      return ret;
    }
  }

  bool Database::pt_mindSplitBase() {
    size_t modsRemaining, i, len;
    if (!pt_targetF) return false;
    if (pt_commitTlog()) return true;
    for (i = 0, modsRemaining = 32, len = atCount;i < len;i++)
      if (!allocTab[i].cacheLocation && allocTab[i].lastWrittenFrame < currentFrame) {
	fread(pt_buffer.raw, sizeof(loadedEntry), 1, pt_activeF);
	fwrite(pt_buffer.raw, sizeof(loadedEntry), 1, pt_targetF);
	allocTab[i].lastWrittenFrame = currentFrame;
	if (!--modsRemaining) break;
      }
    fclose(pt_activeF);
    pt_activeF = pt_targetF;
    pt_targetF = NULL;
    targetFrame = 0;
    target = "";
    return true;
  }

  uint64_t Database::getCurrentFrame() {
    return currentFrame;
  };

}
