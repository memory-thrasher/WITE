/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <string>
#include <map>

#include "stdExtensions.hpp"
#include "shared.hpp"
#include "dbFile.hpp"
#include "dbUtils.hpp"

namespace WITE {

  class dbTableBase {
  public:
    virtual ~dbTableBase() = default;
    //TODO more as needed
  };

  //content is serialized to disk. R is expected to behave when memcpy'd around (no atomics etc).
  template<class R> class dbTable : public dbTableBase {//R for raw
  private:
    typedef R RAW;
    typedef uint64_t U;//underlaying type for raw data, to avoid using constructors and storage qualifiers on disk
    static constexpr size_t TCnt = (sizeof(R) - 1) / sizeof(U) + 1;
    typedef U T[TCnt];
    static constexpr size_t AU = dbAllocationBatchSizeOf<R>::value,
      AU_LOG = dbLogAllocationBatchSizeOf<R>::value;

    enum class eLogType : uint64_t {
      eUpdate,
      eDelete,
    };

    struct D {
      uint64_t firstLog, lastLog, lastDeletedFrame, lastCreatedFrame, lastLogAppliedFrame;
      T data;
    };

    struct L {//every log contains a complete copy
      eLogType type;
      uint64_t frame, previousLog, nextLog;//, masterRow;
      T data;
    };

    const std::filesystem::path mdfFilename, ldfFilename;
    const std::string typeId;
    dbFile<D, AU> masterDataFile;
    dbFile<L, AU_LOG> logDataFile;
    std::map<uint64_t, syncLock> rowLocks;
    syncLock rowLocks_mutex;//only needed for ops that might alter the size of rowLocks

    void appendLog(uint64_t id, L&& l) {
      D& master = masterDataFile.deref(id);
      if(master.lastLog != NONE && logDataFile.deref(master.lastLog).type == eLogType::eDelete) [[unlikely]] {
	//edge case: if writing to a object that has already been deleted based on pre-deletion frame data, just drop the write
	//the object will not be reallocated until after the delete log is applied
	return;
      }
      WITE_DEBUG_DB_MASTER(id);
      uint64_t nlid = logDataFile.allocate();
      L& nl = logDataFile.deref(nlid);
      nl = l;
      nl.nextLog = NONE;
      nl.previousLog = master.lastLog;//might be NONE
      if(master.firstLog == NONE) [[unlikely]] {
	ASSERT_TRAP(master.lastLog == NONE, "invalid log list state");
	master.firstLog = nlid;
      } else {
	ASSERT_TRAP(master.lastLog != NONE, "invalid log list state");
	logDataFile.deref(master.lastLog).nextLog = nlid;
      }
      // nl.masterRow = id;
      master.lastLog = nlid;
      WITE_DEBUG_DB_MASTER(id);
      WITE_DEBUG_DB_LOG(master.lastLog);
    };

    void write(uint64_t id, uint64_t frame, R* data) {
      L log {
	.type = eLogType::eUpdate,
	.frame = frame,
      };
      memcpy(log.data, *data);
      appendLog(id, std::move(log));
    };

  public:

    dbTable(const std::filesystem::path& basedir, const std::string& typeId, bool clobberMaster, bool clobberLog) :
      mdfFilename(basedir / concat({ "master_", typeId, ".wdb" })),
      ldfFilename(basedir / concat({ "log_", typeId, ".wdb" })),
      typeId(typeId),
      masterDataFile(mdfFilename, clobberMaster),
      logDataFile(ldfFilename, clobberLog)
    {
      if(clobberLog && !clobberMaster) { //if we're not keeping the log, drop any references to it
	for(uint64_t id : masterDataFile) {
	  D& m = masterDataFile.deref(id);
	  m.firstLog = m.lastLog = NONE;
	}
      }
    };

    //TODO integrate rollback into constructor if log is not clobbered and exists (a graceful shutdown will delete the log file)
    // void rollback(uint64_t maxFrame) {//trim bits of log from final (possibly incomplete) frame (only use when loading)
    //   if(!clobber && maxFrame) {
    // 	for(uint64_t id : masterDataFile) {
    // 	  D& master = masterDataFile.deref(id);
    // 	  if(master.lastLogAppliedFrame < master.lastCreatedFrame &&
    // 	     (master.firstLog == NONE || logDataFile.deref(master.firstLog).frame > maxFrame)) [[unlikely]] {
    // 	    //file corruption edge case, the first update log was trimmed so the object never got initialized
    // 	    while(master.firstLog != NONE) {
    // 	      L& l = logDataFile.deref(master.firstLog);
    // 	      logDataFile.free(master.firstLog);
    // 	      master.firstLog = l.nextLog;
    // 	    }
    // 	    masterDataFile.free(id);
    // 	    return;
    // 	  }
    // 	  uint64_t tlid = master.lastLog;
    // 	  L* tl = logDataFile.get(tlid);
    // 	  while(tl && tl->frame > maxFrame) {
    // 	    logDataFile.free(tlid);
    // 	    tlid = tl->previousLog;
    // 	    tl = logDataFile.get(tlid);
    // 	  }
    // 	  if(tl) [[likely]]
    // 	    master.lastLog = tlid;
    // 	  else
    // 	    master.lastLog = master.firstLog = NONE;
    // 	}
    //   }
    // };

    inline syncLock* mutexFor(uint64_t rowIdx) {
      scopeLock l(&rowLocks_mutex);
      return &rowLocks[rowIdx];
    };

    //provided for convenience but NOT used internally. Caller must ensure thread safety in access of individual rows.
    // inline scopeLock&& lock(uint64_t rowIdx) {
    //   return std::move(scopeLock(mutexFor(rowIdx)));
    // };

    //only blocks if underlaying file is busy
    uint64_t allocate(uint64_t frame, R* data) {
      uint64_t ret = masterDataFile.allocate();
      //at this point, all logs from the previous holder have been flushed (or else it wouldn't have been in the pool)
      D& master = masterDataFile.deref(ret);
      master.firstLog = master.lastLog = NONE;
      master.lastCreatedFrame = frame;
      WITE_DEBUG_DB_MASTER(ret);
      write(ret, frame, data);
      return ret;
    };

    //`free` must only be called once for each `allocate`. `store` should never be concurrent with `free` on the same id. `store` should never be called after free on the same id unless that id has since been returned by `allocate`.
    void free(uint64_t id, uint64_t frame) {
      appendLog(id, L { .type = eLogType::eDelete, .frame = frame });
    };

    //reads the state of the requested object as of the requested frame, if possible, or otherwise, the oldest known state
    //returns true if the object exists at the time the chosen state was correct, or false to indicate out was unchanged
    //concurrency allowed with everything but `applyLogs`
    bool load(uint64_t id, uint64_t frame, R* out) {
      const D& master = masterDataFile.deref(id);
      if(master.lastDeletedFrame > master.lastCreatedFrame) [[unlikely]]
	return false; //this case only covers the case when the delete log has been applied
      //start from firstLog so a concurrent write (which will alter lastLog) does not interfere
      //if the concurrent write alters firstLog, it is changing it from NONE to the id of a log which is already valid
      L* tl = logDataFile.get(masterDataFile.deref(id).firstLog);
      if(tl != NULL) [[likely]] {
	L* nextL = logDataFile.get(tl->nextLog);
	while(nextL && nextL->frame <= frame) {
	  tl = nextL;
	  nextL = logDataFile.get(tl->nextLog);
	}
      }
      if(tl) [[likely]] {
	switch(tl->type) {
	case eLogType::eDelete:
	  return false;//...so we need this case for when the delete has not yet been applied
	case eLogType::eUpdate:
	  ::memcpy(reinterpret_cast<void*>(out), reinterpret_cast<void*>(&tl->data), sizeof(R));
	  return true;
	}
      } else {
	::memcpy(reinterpret_cast<void*>(out), reinterpret_cast<const void*>(&master.data), sizeof(R));
	return true;
      }
    };

    //concurrency allowed with `load` only. Any calls to `store` must return before further calls are made to `store` or `free`
    void store(uint64_t id, uint64_t frame, R* in) {
      write(id, frame, in);
    };

    //applies all logs to the given object up to and including any associated with the given frame
    //NOTE: applying a delete log frees the object, which invalidates any iterators pointing at it
    //concurrency never allowed. Game loop should not allow log application to overlap with other game logic
    void applyLogs(uint64_t id, uint64_t throughFrame) {
      WITE_DEBUG_DB_MASTER(id);
      D& master = masterDataFile.deref(id);
      //every log has a complete copy of the data portion so we only need to apply the last and free the ones before it
      uint64_t tlid = master.firstLog;
      L* tl = logDataFile.get(tlid);
      WITE_DEBUG_DB_LOG(tlid);
      if(!tl || tl->frame > throughFrame) [[unlikely]] return;
      L* nl = logDataFile.get(tl->nextLog);
      while(nl && nl->frame <= throughFrame) {
	auto oldtlid = tlid;
	tlid = tl->nextLog;
	WITE_DEBUG_DB_LOG(tlid);
	tl = nl;
	nl = logDataFile.get(tl->nextLog);
	logDataFile.free(oldtlid);
      }
      //if there is a delete log, it will be the last one
      switch(tl->type) {
      case eLogType::eDelete:
	ASSERT_TRAP(tl->nextLog == NONE, "delete is not the last log for that object");
	masterDataFile.free(id);
	break;
      case eLogType::eUpdate: [[likely]]
	memcpy(master.data, tl->data);
	master.firstLog = tl->nextLog;//might be NONE
	master.lastLogAppliedFrame = tl->frame;
	if(nl) [[likely]]
	  nl->previousLog = NONE;
	else
	  master.lastLog = NONE;
	break;
      }
      logDataFile.free(tlid);
      WITE_DEBUG_DB_MASTER(id);
    };

    void applyLogsAll(uint64_t throughFrame) {
      auto it = begin();
      auto e = end();
      while(it != e) {
	applyLogs(*it++, throughFrame);//prefix increment: the iterator must be incremented before applyLogs is called so it doesn't get invalidated by a delete log
      }
    };

    void copyMdf(const std::filesystem::path& outdir) {
      std::filesystem::path outfile = outdir / concat({ "backup_", typeId, ".wdb" });
      masterDataFile.copy(outfile);
    };

    void deleteFiles() {
      logDataFile.close();
      masterDataFile.close();
      std::filesystem::remove(mdfFilename);
      std::filesystem::remove(ldfFilename);
    };

    void deleteLogs() {
      logDataFile.close();
      std::filesystem::remove(ldfFilename);
    };

    inline auto begin() {
      return masterDataFile.begin();
    };

    inline auto end() {
      return masterDataFile.end();
    };

    inline uint64_t capacity() {
      return masterDataFile.capacity();
    };

    inline uint64_t size() {
      return capacity() - masterDataFile.freeSpace();
    };

    uint64_t maxFrame() {
      uint64_t ret = 0;
      for(uint64_t id : masterDataFile)
	ret = max(ret, logDataFile.deref(masterDataFile.deref(id).lastLog).frame);
      return ret;
    };

    uint64_t minFrame() {
      uint64_t ret = NONE;
      for(uint64_t id : masterDataFile)
	ret = min(ret, logDataFile.deref(masterDataFile.deref(id).firstLog).frame);
      return ret;
    };

  };

}
