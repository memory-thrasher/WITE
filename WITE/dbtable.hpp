#pragma once

#include <string>
#include <map>

#include "dbTemplateStructs.hpp"
#include "stdExtensions.hpp"
#include "shared.hpp"

namespace WITE {

  //content is serialized to disk. R is expected to behave when memcpy'd around (no atomics etc).
  template<class R, size_t AU = 128, size_t AU_LOG = AU * 4> class dbtable {//R for raw
  private:
    typedef R RAW;
    typedef uint64_t U;//underlaying type for raw data, to avoid using constructors and storage qualifiers on disk
    static constexpr size_t TCnt = (sizeof(R) - 1) / 8 + 1;
    typedef U T[TCnt];

    enum class eLogType : uint64_t {
      eUpdate,
      eDelete,
    };

    struct D {
      uint64_t firstLog, lastLog, lastDeletedFrame, lastCreatedFrame;
      T data;
    };

    struct L {//every log contains a complete copy
      eLogType type;
      uint64_t frame, previousLog, nextLog;
      T data;
    };

    const std::string mdfFilename, ldfFilename;
    dbfile<D, AU> masterDataFile;
    dbfile<L, AU_LOG> logDataFile;
    std::map<uint64_t, syncLock> rowLocks;
    syncLock rowLocks_mutex;//only needed for ops that might alter the size of rowLocks

    //row mutex must be held
    void appendLog(uint64_t id, L&& l) {
      uint64_t nlid = logDataFile.allocate();
      L& nl = logDataFile.deref(nlid);
      nl = l;
      D& master = masterDataFile.deref(id);
      if(master.firstLog == NONE) [[unlikely]] {
	ASSERT_TRAP(master.lastLog == NONE, "invalid log list state");
	master.firstLog = nlid;
      } else {
	ASSERT_TRAP(master.lastLog != NONE, "invalid log list state");
	logDataFile.deref(master.lastLog).nextLog = nlid;
      }
      nl.previousLog = master.lastLog;//might be NONE
      nl.nextLog = NONE;
      master.lastLog = nlid;
    };

    inline const L* findLastLogBefore(uint64_t id, uint64_t frame) {
      //requests will generally be for the previous frame's data. If there's a ton of logs, most are probably older.
      //also, if multiple writes happen on the same frame (which should be avoided) this will get the last one
      uint64_t tlid = masterDataFile.deref(id).lastLog;
      const L* tl;
      do {
	if(tlid == NONE) [[unlikely]] return NULL;
	tl = &logDataFile.deref(tlid);
	tlid = tl->previousLog;
      } while(tl->frame > frame);
      return tl;
    };

    //row mutex must be held
    bool read(uint64_t id, uint64_t frame, R* out) {
      const D& master = masterDataFile.deref(ret);
      if(master.lastDeletedFrame > lastCreatedFrame) [[unlikely]]
	return false; //this case only covers the case when the delete log has been applied
      const L* lastLog = findLastLogBefore(id, frame);
      if(lastLog) [[likely]] {
	switch(lastLog.type) {
	case eLogType::eDelete:
	  return false;//...so we need this case when the delete has not yet been applied
	case eLogType::eUpdate:
	  memcpy(*out, lastLog.data);
	  return true;
	}
      } else {
	memcpy(*out, master.data);
	return true;
      }
    };

    //row mutex must be held
    void write(uint64_t id, uint64_t frame, R* data) {
      L log {
	.type = eLogType::eUpdate,
	.frame = frame,
      };
      memcpy(log.data, *data);
      appendLog(id, log);
    };

  public:

    dbtable(std::string basedir, std::string typeId, bool clobber = false) :
      mdfFilename(concat(std::string[]{ basedir, "master_", typeId, ".wdb" })),
      ldfFilename(concat(std::string[]{ basedir, "log_", typeId, ".wdb" })),
      masterDataFile(mdfFilename, clobber),
      logDataFile(ldfFilename, clobber)
    {};

    void rollback(uint64_t maxFrame) {//trim bits of log from final (possibly incomplete) frame (only use when loading)
      if(!clobber && maxFrame) {
	for(uint64_t id : masterDataFile) {
	  D& master = masterDataFile.deref(ret);
	  while(master.lastLog != NONE) {
	    L& l = logDataFile.deref(master.lastLog);
	    if(l <= maxFrame) break;
	    master.lastLog = l.previousLog;
	  }
	}
      }
    };

    inline syncLock* mutexFor(uint64_t rowIdx) {
      scopeLock l(&rowLocks_mutex);
      return rowLocks[rowIdx];
    };

    inline scopeLock&& lock(uint64_t rowIdx) {
      return std::move(scopeLock(mutexFor(rowIdx)));
    };

    uint64_t allocate(uint64_t frame, R* data) {
      uint64_t ret = masterDataFile.allocate();
      scopeLock l = lock(ret);
      //at this point, all logs from the previous holder have been flushed (or else it wouldn't have been in the pool)
      D& master = masterDataFile.deref(ret);
      master.firstLog = master.lastLog = NONE;
      master.lastCreatedFrame = frame;
      write(ret, frame, data);
      return ret;
    };

    void free(uint64_t id, uint64_t frame) {
      scopeLock l = lock(id);
      appendLog(id, L { .type = eLogType::eDelete, .frame = frame });
    };

    //reads the state of the requested object as of the requested frame, if possible, or otherwise, the oldest known state
    //returns true if the object exists at the time the chosen state was correct, or false to indicate out was unchanged
    bool consume(uint64_t id, uint64_t frame, R* out) {
      scopeLock l = lock(id);
      return read(id, frame, out);
    };

    void store(uint64_t id, uint64_t frame, R* in) {
      scopeLock l = lock(id);
      write(id, frame, in);
    };

  };

}
