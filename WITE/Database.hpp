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

#include "SyncLock.hpp"
#include "DBDelta.hpp"
#include "DBRecord.hpp"

#ifndef DB_THREAD_COUNT
#define DB_THREAD_COUNT 16
#endif

namespace WITE::DB {

  class DBThread;
  class DBDelta;
  enum DBThread::semaphoreState;
  class DBEntity;

  class Database {
  private:
    uint64_t currentFrame = 0;
    volatile bool shuttingDown = false, started = false;
    std::unique_ptr<DBThread[DB_THREAD_COUNT]> threads;
    std::map<int, size_t> threadsByTid;
    SyncLock logMutex;
    RollingQueue<DBDelta, DB_THREAD_COUNT * 256 * 1024 * 1024 / sizeof(DBDelta)> log;
    AtomicRollingQueue<DBEntity*> free;
    size_t entityCount;
    const std::unique_ptr<DBEntity[]> metadata;//TODO make this expandable IF a backing file was provided?
    DBRecord* data;//mmap'ed file
    //bool anyThreadIs(DBThread::semaphoreState);
    void applyLogTransactions(size_t min = 0);//applying a minimum of 0 attempts one batch
    bool deltaIsInPast(DBDelta*);
    static Callback_t<bool, DBDelta*> deltaIsInPast_cb;
    Database();//boilerplate
    DBThread* getLightestThread();
  public:
    Database(int backingStore_fd, size_t entityCount = 0);//entityCount intended for making a new file
    Database(size_t entityCount);//for anonymous
    // void mainLoop();
    void write(DBDelta*);//appends to the current thread's transaction queue, flushing it if out of space
    void read(DBEntity* src, DBRecord* dst);
    DBEntity* getEntity(size_t id);
    uint64_t getFrame() { return currentFrame; };
    DBEntity* allocate(size_t count = 1);
    void deallocate(DBEntity*, DBRecord* info = NULL);//requires some info from the record's present state, provide if already read to avoid redundent read
  }

}

