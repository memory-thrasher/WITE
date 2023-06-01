/*
new database:
one slice per thread
in-ram table of objects (made of DBEntity instances), always readable by all threads (no resizing!)
objects needn't be aware of which thread they are on

maybe:
automatic transfer of objects between thread slices
every thread can create objects in their own slice any time without locking
slice is fixed maximum number of objects
number of threads can be changed at runtime but expect lag (user setting or similar, stop all threads, realloc and spawn new)

 */

#pragma once

#include <map>
#include <memory>

#include "IteratorMultiplexer.hpp"
#include "SyncLock.hpp"
#include "DBRecord.hpp"
#include "DBThread.hpp"
#include "entity_type.hpp"
#include "constants.hpp"

namespace WITE::DB {

  class DBEntity;
  class DBDelta;

  class Database {
  private:
    uint64_t currentFrame = 0;
    std::atomic<bool> shuttingDown = false, started = false;
    DBThread* threads;
    std::map<int, size_t> threadsByTid;
    size_t entityCount;
    std::queue<DBEntity*> free;
    Util::SyncLock free_mutex;
    typedefCB(deltaIsInPast_cb_t, bool, const DBDelta*)
    deltaIsInPast_cb_t deltaIsInPast_cb;
    DBEntity* metadata;//TODO make this expandable IF a backing file was provided?
    union {//mmap'ed file
      DBRecord* data;
      void* data_raw;
    };
    std::map<DBRecord::type_t, struct entity_type> types;
    bool anyThreadIs(DBThread::semaphoreState);
    bool anyThreadBroke();
    void signalThreads(DBThread::semaphoreState from, DBThread::semaphoreState to, DBThread::semaphoreState waitFor);
    size_t applyLogTransactions(DBThread*);
    bool deltaIsInPast(const DBDelta*);
    DBThread* getLightestThread();
    DBThread* getCurrentThread();
    void stop();
    static Database* liveDb;
  public:
    Database(struct entity_type* types, size_t typeCount, size_t entityCount, int backingStore_fd = -1);
    ~Database();
    // void mainLoop();
    void write(DBDelta*);//appends to the current thread's transaction queue, flushing it if out of space
    void read(DBEntity* src, DBRecord* dst);
    void read(DBEntity* src, DBRecord* dst, size_t offset, size_t len);
    void readHeader(DBEntity* src, DBRecord::header_t* out, bool readNext = true, bool readType = true, bool readFlags = true);
    DBEntity* getEntity(size_t id);
    DBEntity* getEntityAfter(DBEntity* src);
    uint64_t getFrame() { return currentFrame; };
    size_t getEntityCount();
    DBEntity* allocate(DBRecord::type_t type, size_t count = 1, bool isHead = true);
    inline DBEntity* allocate(const struct entity_type& type, size_t count = 1) { return allocate(type.typeId, count); };
    void deallocate(DBEntity*, DBRecord::header_t* info = NULL);//requires some info from the record's present state, provide if already read to avoid redundent read
    const struct entity_type* getType(DBRecord::type_t);
    void start();
    void shutdown();
    void idle(DBThread*);//for worker threads to be productive while blocked

    using threadTypeSearchIterator = typename Collections::IteratorMultiplexer<DBThread*, decltype(DBThread::typeIndex)::mapped_type::iterator>;
    threadTypeSearchIterator getByType(DBRecord::type_t type) const;
    inline size_t countByType(DBRecord::type_t t) const { return getByType(t).count(); };

  };

}

