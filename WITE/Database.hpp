/*
new database:
one slice per thread
every thread can create objects in their own slice any time without locking
number of threads can be changed at runtime but expect lag (user setting or similar, stop all threads, realloc and spawn new)
slice is fixed maximum number of objects
in-ram table of objects (made of DBEntity instances), always readable by all threads (no resizing!)
objects needn't be aware of which thread they are on
if writes on the same frame overlap, call the associated resolution callback

maybe:
automatic transfer of objects between thread slices

 */

#pragma once

#include <map>
#include <memory>

#include "IteratorMultiplexer.hpp"
#include "IteratorWrapper.hpp"
#include "SyncLock.hpp"
#include "DBDelta.hpp"
#include "DBRecord.hpp"
#include "AtomicRollingQueue.hpp"
#include "DBThread.hpp"

#ifndef DB_THREAD_COUNT
#define DB_THREAD_COUNT 16
#endif

namespace WITE::DB {

  class DBEntity;

  class Database {
  private:
    uint64_t currentFrame = 0;
    volatile bool shuttingDown = false, started = false;
    DBThread* threads;
    std::map<int, size_t> threadsByTid;
    Util::SyncLock logMutex;
    Collections::RollingQueue<DBDelta, DB_THREAD_COUNT * 256l * 1024 * 1024 / sizeof(DBDelta)> log;
    size_t entityCount;
    std::unique_ptr<Collections::AtomicRollingQueue<DBEntity*>> free;
    std::unique_ptr<Util::Callback_t<bool, DBDelta*>> deltaIsInPast_cb;
    DBEntity* metadata;//TODO make this expandable IF a backing file was provided?
    union {//mmap'ed file
      DBRecord* data;
      void* data_raw;
    };
    std::map<DBRecord::type_t, struct entity_type> types;
    bool anyThreadIs(DBThread::semaphoreState);
    bool anyThreadBroke();
    void signalThreads(DBThread::semaphoreState from, DBThread::semaphoreState to, DBThread::semaphoreState waitFor);
    size_t applyLogTransactions(size_t min = 0);//applying a minimum of 0 attempts one batch
    bool deltaIsInPast(DBDelta*);
    //TODO (head) entities by type, liked list?
    DBThread* getLightestThread();
    DBThread* getCurrentThread();
    void stop();
  public:
    Database(struct entity_type* types, size_t typeCount, size_t entityCount, int backingStore_fd = -1);
    ~Database();
    // void mainLoop();
    void write(DBDelta*);//appends to the current thread's transaction queue, flushing it if out of space
    void read(DBEntity* src, DBRecord* dst);
    DBEntity* getEntity(size_t id) { return &metadata[id]; };
    DBEntity* getEntityAfter(DBEntity* src);
    uint64_t getFrame() { return currentFrame; };
    DBEntity* allocate(DBRecord::type_t type, size_t count = 1, bool isHead = true);
    inline DBEntity* allocate(const struct entity_type& type, size_t count = 1) { return allocate(type.typeId, count); };
    void deallocate(DBEntity*, DBRecord* info = NULL);//requires some info from the record's present state, provide if already read to avoid redundent read
    const struct entity_type* getType(DBRecord::type_t);
    void start();
    void shutdown();
    void flushTransactions(decltype(DBThread::transactions)*);

    const static auto getByTypeThread_cb = Collections::IteratorMultiplexer<DBThread*, decltype(DBThread::typeIndex)::mapped_type::iterator>::fetcher_cb_F::make_membercaller(&DBThread::getSliceMembersOfType);
    auto getByType(DBRecord::type_t type) const {
      return Collections::IteratorMultiplexer//<DBThread*, decltype(DBThread::typeIndex)::mapped_type::iterator>
	(Collections::IteratorWrapper(threads, threads + DB_THREAD_COUNT + 1), getByTypeThread_cb);
    };

  };

}

