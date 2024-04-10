#pragma once

#include <string>
#include <map>

#include "dbTemplateStructs.hpp"
#include "stdExtensions.hpp"
#include "shared.hpp"

namespace WITE {

  class dbtableBase {
  public:
    virtual ~dbtableBase() = default;
    //TODO more as needed
  };

  //content is serialized to disk. R is expected to behave when memcpy'd around (no atomics etc).
  template<class R, size_t AU = 128, size_t AU_LOG = AU * 4> class dbtable : public dbtableBase {//R for raw
  private:
    typedef R RAW;
    typedef uint64_t U;//underlaying type for raw data, to avoid using constructors and storage qualifiers on disk
    static constexpr size_t TCnt = (sizeof(R) - 1) / sizeof(U) + 1;
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
      uint64_t frame, previousLog, nextLog;//, masterRow;
      T data;
    };

    const std::string mdfFilename, ldfFilename;
    dbfile<D, AU> masterDataFile;
    dbfile<L, AU_LOG> logDataFile;
    std::map<uint64_t, syncLock> rowLocks;
    syncLock rowLocks_mutex;//only needed for ops that might alter the size of rowLocks

    //row mutex must be held
    void appendLog(uint64_t id, L&& l) {
      D& master = masterDataFile.deref(id);
      if(master.lastLog == NONE && logDataFile.deref(master.lastLog).type == eLogType::eDelete) [[unlikely]] {
	//edge case: if writing to a object that has already been deleted based on pre-deletion frame data, just drop the write
	//the object will not be reallocated until after the delete log is applied
	return;
      }
      uint64_t nlid = logDataFile.allocate();
      L& nl = logDataFile.deref(nlid);
      nl = l;
      if(master.firstLog == NONE) [[unlikely]] {
	ASSERT_TRAP(master.lastLog == NONE, "invalid log list state");
	master.firstLog = nlid;
      } else {
	ASSERT_TRAP(master.lastLog != NONE, "invalid log list state");
	logDataFile.deref(master.lastLog).nextLog = nlid;
      }
      nl.previousLog = master.lastLog;//might be NONE
      nl.nextLog = NONE;
      // nl.masterRow = id;
      master.lastLog = nlid;
    };

    //row mutex must be held
    inline const L* findLastLogBefore(uint64_t id, uint64_t frame) {
      //requests will generally be for the previous frame's data. If there's a ton of logs, most are probably older.
      //also, if multiple writes happen on the same frame (which should be avoided) this will get the last one
      const L* tl = logDataFile.get(masterDataFile.deref(id).lastLog);
      while(tl && tl->frame > frame)
	tl = logDataFile.get(tl->previousLog);
      return tl;
    };

    //row mutex must be held
    bool read(uint64_t id, uint64_t frame, R* out) {
      const D& master = masterDataFile.deref(ret);
      if(master.lastDeletedFrame > lastCreatedFrame) [[unlikely]]
	return false; //this case only covers the case when the delete log has been applied
      const L* lastLog = findLastLogBefore(id, frame);
      if(lastLog) [[likely]] {
	switch(lastLog->type) {
	case eLogType::eDelete:
	  return false;//...so we need this case when the delete has not yet been applied
	case eLogType::eUpdate:
	  memcpy(*out, lastLog->data);
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

    dbtable(std::string& basedir, std::string& typeId, bool clobber = false) :
      mdfFilename(concat(std::string[]{ basedir, "master_", typeId, ".wdb" })),
      ldfFilename(concat(std::string[]{ basedir, "log_", typeId, ".wdb" })),
      masterDataFile(mdfFilename, clobber),
      logDataFile(ldfFilename, clobber)
    {};

    void rollback(uint64_t maxFrame) {//trim bits of log from final (possibly incomplete) frame (only use when loading)
      if(!clobber && maxFrame) {
	for(uint64_t id : masterDataFile) {
	  D& master = masterDataFile.deref(ret);
	  uint64_t tlid = master.lastLog;
	  L* tl = logDataFile.get(tlid);
	  while(tl && tl->frame > maxFrame) {
	    logDataFile.free(tlid);
	    tlid = tl->previousLog;
	    tl = logDataFile.get(tlid);
	  }
	  if(tl) [[likely]]
	    master.lastLog = tlid;
	  else
	    master.lastLog = master.firstLog = NONE;
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

    //applies all logs to the given object up to and including any associated with the given frame
    //NOTE: applying a delete log frees the object, which invalidates any iterators pointing at it
    void applyLogs(uint64_t id, uint64_t throughFrame) {
      scopeLock lk = lock(id);
      D& master = masterDataFile.deref(ret);
      //every log has a complete copy of the data portion so we only need to apply the last and free the ones before it
      uint64_t tlid = master.firstLog;
      L* tl = logDataFile.get(tlid);
      if(!tl || tl->frame > throughFrame) [[unliekly]] return;
      L* nl = logDataFile.get(tl->nextLog);
      while(nl && nl->frame <= throughFrame) {
	logDataFile.free(tlid);
	tlid = tl->nextLog;
	tl = nl;
	nl = logDataFile.get(tl->nextLog);
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
	if(nl) [[likely]]
	  nl->previousLog = NONE;
	else
	  master.lastLog = NONE;
	break;
      }
      logDataFile.free(tlid);
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

  };

}
