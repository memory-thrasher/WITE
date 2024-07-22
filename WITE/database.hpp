/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include "dbUtils.hpp"
#include "dbTableTuple.hpp"

namespace WITE {

  /*each type given to the db must have the following static members or typedefs:
    uint64_t typeId
    std::string dbFileId
optional members:
    update(uint64_t objectId, void* db) //called every frame
    allocated(uint64_t objectId, void* db) //called when object is first created (should init persistent data)
    freed(uint64_t objectId, void* db) //called when object is destroyed (rare, should free db children)
    spunUp(uint64_t objectId, void* db) //called when object is created or loaded from disk (should set up any vulkan or other transients)
    spunDown(uint64_t objectId, void* db) //called when the object is destroyed or when the game is closing (should clean up transients)
   */

  //each type is stored as-is on disk (memcpy and mmap) so should be simple. POD except for static members is recommended.
  template<class... TYPES> class database {
  private:
    static constexpr size_t tableCount = sizeof...(TYPES);
    static_assert(tableCount > 0);
    std::atomic_uint64_t currentFrame;
    dbTableTuple<TYPES...> bobby;//327
    threadPool threads;//dedicated thread pool so we can tell when all frame data is done
    std::atomic_bool backupInProgress;
    std::string backupTarget;
    thread* backupThread = NULL;
    std::atomic_uint64_t tablesBackedUp;

    template<class T, class... REST> inline void applyLogsThrough(uint64_t applyFrame) {
      bobby.template get<T::typeId>().applyLogsAll(applyFrame);
      if constexpr(sizeof...(REST) > 0)
	applyLogsThrough<REST...>(applyFrame);
    };

    template<class T, class... REST> inline void backupTable(uint64_t applyFrame) {
      //log files won't be backed up, and won't be applied while the backup is running, so just get all the mdfs to a common frame and let all new data flow sit around in the logs
      auto& tbl = bobby.template get<T::typeId>();
      tbl.applyLogsAll(applyFrame);
      tbl.copyMdf(backupTarget);
      tablesBackedUp.fetch_add(1, std::memory_order_relaxed);
      if constexpr(sizeof...(REST) > 0)
	backupTable<REST...>(applyFrame);
    };

    void backupThreadEntry() {
      uint64_t applyFrame = maxFrame() - 1;
      uint32_t sleepCnt = 0;
      while(minFrame() >= applyFrame) {//need more log variety to ensure only a complete frame is written
	thread::sleepShort(sleepCnt);
	applyFrame = maxFrame() - 1;
      }
      backupTable<TYPES...>(applyFrame);
      backupInProgress.store(false, std::memory_order_release);
    };

    template<class A, class... REST> inline uint64_t maxFrame() {
      uint64_t ret = bobby.template get<A::typeId>().maxFrame();
      if constexpr(sizeof...(REST) > 0)
	ret = max(ret, maxFrame<REST...>());
      return ret;
    };

    template<class A, class... REST> inline uint64_t minFrame() {
      uint64_t ret = bobby.template get<A::typeId>().minFrame();
      if constexpr(sizeof...(REST) > 0)
	ret = min(ret, minFrame<REST...>());
      return ret;
    };

    template<class A, class... REST> inline void updateAll() {
      if constexpr(has_update<A>::value) {
	auto& tbl = bobby.template get<A::typeId>();
	auto iter = tbl.begin();
	auto end = tbl.end();
	while(iter != end) {
	  uint64_t oid = *(iter++);
	  //NOTE: postfix operator, iterator MUST be incremented before update is called or else the iterator might be invalidadted if the object deletes itself in its own update (which is the recommended place to delete something).
	  dbJobWrapper<A, A::update>(oid, this, threads);
	}
      }
      if constexpr(sizeof...(REST) > 0)
	updateAll<REST...>();
    };

    template<class A, class... REST> inline void spinUpAll() {
      if constexpr(has_spunUp<A>::value)
	for(uint64_t oid : bobby.template get<A::typeId>())
	  dbJobWrapper<A, A::spunUp>(oid, this, threads);
      if constexpr(sizeof...(REST) > 0)
	spinUpAll<REST...>();
    };

    template<class A, class... REST> inline void spinDownAll() {
      if constexpr(has_spunDown<A>::value)
	for(uint64_t oid : bobby.template get<A::typeId>())
	  dbJobWrapper<A, A::spunDown>(oid, this, threads);
      if constexpr(sizeof...(REST) > 0)
	spinDownAll<REST...>();
    };

    template<class A, class... REST> inline void deleteFiles() {
      bobby.template get<A::typeId>().deleteFiles();
      if constexpr(sizeof...(REST) > 0)
	deleteFiles<REST...>();
    };

    template<class A, class... REST> inline void deleteLogs() {
      bobby.template get<A::typeId>().deleteLogs();
      if constexpr(sizeof...(REST) > 0)
	deleteLogs<REST...>();
    };

  public:
    database(const std::filesystem::path& basedir, bool clobberMaster, bool clobberLog) : bobby(basedir, clobberMaster, clobberLog) {
      ASSERT_TRAP(clobberLog || !clobberMaster, "cannot keep log without master");
      currentFrame = maxFrame() + 1;
      spinUpAll<TYPES...>();
    };

    uint64_t maxFrame() {
      return maxFrame<TYPES...>();
    };

    uint64_t minFrame() {
      return minFrame<TYPES...>();
    };

    //the intention is for main to call in a loop: winput.poll(); db.updateTick(); onion.render(); db.endFrame();

    //process a single frame, part 1: updates only
    void updateTick() {
      threads.waitForAll();
      updateAll<TYPES...>();
    };

    //process a single frame, part 2: logfile maintenance
    void endFrame() {
      threads.waitForAll();
      if(!backupInProgress.load(std::memory_order_consume)) { //don't flush logs when the mdfs are being copied
	if(backupThread) {
	  //all threads should be joined once they're finished
	  backupThread->join();//should be immediately joinable since it's done (or very nearly so)
	  backupThread = NULL;
	}
	if(currentFrame > MIN_LOG_HISTORY)
	  applyLogsThrough<TYPES...>(currentFrame - MIN_LOG_HISTORY);
      }
      currentFrame.fetch_add(1, std::memory_order_relaxed);
    };

    bool requestBackup(const std::string& outdir) {
      bool t = false;
      std::error_code ec;
      if(!std::filesystem::exists(outdir))
	ASSERT_TRAP_OR_RUN(std::filesystem::create_directories(outdir, ec), "create dir failed ", ec);
      ASSERT_TRAP(std::filesystem::is_directory(outdir, ec), "not a directory ", ec);
      if(backupInProgress.compare_exchange_strong(t, true, std::memory_order_acq_rel)) {
	tablesBackedUp.store(0, std::memory_order_relaxed);
	backupTarget = outdir;
	if(backupThread) {
	  //all threads should be joined once they're finished
	  backupThread->join();//should be immediately joinable since it's done (or very nearly so)
	  backupThread = NULL;
	}
	//callback allocation is forgivable for file io
	backupThread = thread::spawnThread(thread::threadEntry_t_F::make(this, &database::backupThreadEntry));
	return true;
      } else {
	return false;
      }
    };

    void deleteFiles() {
      deleteFiles<TYPES...>();
    }

    void deleteLogs() {
      deleteLogs<TYPES...>();
    }

    void gracefulShutdown() {
      ASSERT_TRAP(currentFrame > 0, "cannot shutdown a db on frame 0");
      threads.waitForAll();
      uint32_t sleepCnt = 0;
      while(backupInProgress.load(std::memory_order_consume)) thread::sleepShort(sleepCnt);
      applyLogsThrough<TYPES...>(currentFrame - 1);
      spinDownAll<TYPES...>();
      threads.waitForAll();
      deleteLogs();
    };

    template<class A> uint64_t create(A* data) {
      uint64_t ret = bobby.template get<A::typeId>().allocate(currentFrame, data);
      if constexpr(has_allocated<A>::value)
	A::allocated(ret, this);
      if constexpr(has_spunUp<A>::value)
	A::spunUp(ret, this);
      return ret;
    };

    template<class A> void destroy(uint64_t oid) {
      bobby.template get<A::typeId>().free(oid, currentFrame);
      if constexpr(has_spunDown<A>::value)
	A::spunDown(oid, this);
      if constexpr(has_freed<A>::value)
	A::freed(oid, this);
    };

    //true if object exists and was copied to `out`, false otherwise
    //does NOT lock the row, even when reading the current frame, use with caution
    template<class A> inline bool read(uint64_t oid, uint64_t frameDelay, A* out) {
      return bobby.template get<A::typeId>().load(oid, currentFrame - frameDelay, out);
    };

    //true if object exists and was copied to `out`, false otherwise
    //locks the object
    template<class A> inline bool readCurrent(uint64_t oid, A* out) {
      scopeLock lock = bobby.template get<A::typeId>().mutexFor(oid);
      return read(oid, 0, out);
    };

    //true if object exists and was copied to `out`, false otherwise
    //usually non-blocking, safe because prior frame data does not change
    template<class A> inline bool readCommitted(uint64_t oid, A* out) {
      return read(oid, 1, out);
    };

    //true if object exists and was copied to `out`, false otherwise
    //does NOT lock the row, externally lock if more than one write might happen in a frame.
    template<class A> inline void write(uint64_t oid, A* in) {
      return bobby.template get<A::typeId>().store(oid, currentFrame, in);
    };

    //returns a lock object representing the single object to ensure sequential io to that object
    template<class A> inline syncLock* mutexFor(uint64_t oid) {
      return bobby.template get<A::typeId>().mutexFor(oid);
    };

    inline uint64_t getFrame() {
      return currentFrame;
    };

  };

};
