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

#include "SyncLock.hpp"
#include "DBDelta.hpp"
#include "DBRecord.hpp"
#include "AtomicRollingQueue.hpp"
#include "DBThread.hpp"

#ifndef DB_THREAD_COUNT
#define DB_THREAD_COUNT 16
#endif

namespace WITE::DB {

  class DBDelta;
  class DBEntity;
  struct entity_type;

  class Database {
  private:
    uint64_t currentFrame = 0;
    volatile bool shuttingDown = false, started = false;
    std::unique_ptr<DBThread[DB_THREAD_COUNT]> threads;
    std::map<int, size_t> threadsByTid;
    Util::SyncLock logMutex;
    Collections::RollingQueue<DBDelta, DB_THREAD_COUNT * 256l * 1024 * 1024 / sizeof(DBDelta)> log;
    Collections::AtomicRollingQueue<DBEntity*> free;
    size_t entityCount;
    const std::unique_ptr<DBEntity[]> metadata;//TODO make this expandable IF a backing file was provided?
    union {
      DBRecord* data;//mmap'ed file
      void* data_raw;
    };
    bool anyThreadIs(DBThread::semaphoreState);
    bool anyThreadBroke();
    void signalThreads(DBThread::semaphoreState);
    void applyLogTransactions(size_t min = 0);//applying a minimum of 0 attempts one batch
    bool deltaIsInPast(DBDelta*);
    static Util::Callback_t<bool, DBDelta*> deltaIsInPast_cb;
    Database(struct entity_type* types, size_t typeCount);//boilerplate
    std::map<DBRecord::type_t, struct entity_type> types;
    //TODO (head) entities by type, liked list?
    DBThread* getLightestThread();
    DBThread* getCurrentThread();
  public:
    Database(struct entity_type* types, size_t typeCount,
	     int backingStore_fd, size_t entityCount = 0);//entityCount intended for making a new file
    Database(struct entity_type* types, size_t typeCount, size_t entityCount);//for anonymous
    // void mainLoop();
    void write(DBDelta*);//appends to the current thread's transaction queue, flushing it if out of space
    void read(DBEntity* src, DBRecord* dst);
    DBEntity* getEntity(size_t id);
    uint64_t getFrame() { return currentFrame; };
    DBEntity* allocate(DBRecord::type_t type, size_t count = 1, bool isHead = true);
    void deallocate(DBEntity*, DBRecord* info = NULL);//requires some info from the record's present state, provide if already read to avoid redundent read
    const struct entity_type* getType(DBRecord::type_t);
    void start();
  };

}

