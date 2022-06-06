#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "Database.hpp"
#include "DBEntity.hpp"
#include "DBRecord.hpp"
#include "DBDelta.hpp"
#include "DBThread.hpp"
#include "DEBUG.hpp"

namespace WITE::DB {

  void Database::constructorCommon(struct entity_type* types, size_t typeCount) {
    deltaIsInPast_cb = Util::CallbackFactory<bool, DBDelta*>::make_unique(this, &Database::deltaIsInPast);
    threads = reinterpret_cast<DBThread*>(malloc(sizeof(DBThread) * DB_THREAD_COUNT));
    if(!threads)
      CRASH_PREINIT("Failed to create thread array");
    for(size_t i = 0;i < DB_THREAD_COUNT;i++)
      new(&threads[i])DBThread(this, i);
    for(size_t i = 0;i < typeCount;i++) {
      this->types.insert({types[i].typeId, types[i]});
    }
  }

  Database::Database(struct entity_type* types, size_t typeCount, size_t entityCount) :
    entityCount(entityCount), free(std::make_unique<Collections::AtomicRollingQueue<DBEntity*>>(entityCount)) {
    constructorCommon(types, typeCount);
    DBEntity* entities = reinterpret_cast<DBEntity*>(malloc(sizeof(DBEntity) * entityCount));
    for(size_t i = 0;i < entityCount;i++)
      free->push(new(&entities[i])DBEntity(this, i));
    //no need to load anything when there's no file to load from
    this->metadata = entities;
    data_raw = mmap(NULL, sizeof(DBRecord) * entityCount, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | (23 << MAP_HUGE_SHIFT), -1, 0);
  }

  Database::Database(struct entity_type* types, size_t typeCount, int backingStoreFd, size_t entityCount) {
    constructorCommon(types, typeCount);
    if(!entityCount) {
      struct stat stat;
      fstat(backingStoreFd, &stat);
      entityCount = stat.st_size / sizeof(DBRecord);
    }
    this->entityCount = entityCount;
    free = std::make_unique<Collections::AtomicRollingQueue<DBEntity*>>(entityCount);
    data_raw = mmap(NULL, sizeof(DBRecord) * entityCount, PROT_READ | PROT_WRITE,
		    MAP_SHARED | MAP_HUGETLB | (23 << MAP_HUGE_SHIFT) | MAP_POPULATE, backingStoreFd, 0);
    DBEntity* entities = reinterpret_cast<DBEntity*>(malloc(sizeof(DBEntity) * entityCount));
    for(size_t i = 0;i < entityCount;i++) {
      //TODO progress report callback for loading bar?
      DBEntity* dbe = new(&entities[i])DBEntity(this, i);
      if((data[i].header.flags & DBRecordFlag::allocated) != 0) {
	size_t tid = i % DB_THREAD_COUNT;
	if(DBEntity::isUpdatable(&data[i], this)) {
	  dbe->masterThread = tid;
	  threads[tid].addToSlice(dbe);
	}
      } else {
	free->push(dbe);
      }
    }
    this->metadata = entities;
  }

  Database::~Database() {
    stop();
    delete [] threads;
    delete [] metadata;
    while(applyLogTransactions());
    msync(data_raw, sizeof(DBRecord) * entityCount, MS_SYNC);//flush
    munmap(data_raw, sizeof(DBRecord) * entityCount);
  }

  bool Database::anyThreadIs(DBThread::semaphoreState state) {
    for(size_t i = 0;i < DB_THREAD_COUNT;i++)
      if(threads[i].semaphore == state)
	return true;
    return false;
  }

  bool Database::anyThreadBroke() {
    for(size_t i = 0;i < DB_THREAD_COUNT;i++) {
      auto state = threads[i].semaphore.load();
      if(state == DBThread::state_exited || state == DBThread::state_exiting || state == DBThread::state_exploded)
	return true;
    }
    return false;
  }

  void Database::signalThreads(DBThread::semaphoreState s) {
    for(size_t i = 0;i < DB_THREAD_COUNT;i++)
      threads[i].setState(s);
  }

  void Database::start() {
    started = true;
    for(size_t i = 0;i < DB_THREAD_COUNT;i++) {
      threads[i].start();
      threadsByTid.insert({ threads[i].getTid(), i });
    }
    while(!shuttingDown) {
      while(anyThreadIs(DBThread::state_updating))
	WITE::Platform::Thread::sleepShort();
      if(anyThreadBroke()) {
	stop();
	break;
      }
      signalThreads(DBThread::state_maintaining);
      while(anyThreadIs(DBThread::state_maintaining))
	WITE::Platform::Thread::sleepShort();
      if(anyThreadBroke()) {
	stop();
	break;
      }
      signalThreads(DBThread::state_updating);
    }
  }

  void Database::stop() {
    shuttingDown = true;
    signalThreads(DBThread::state_exiting);
    for(size_t i = 0;i < DB_THREAD_COUNT;i++)
      threads[i].join();
  }

  DBThread* Database::getLightestThread() {
    DBThread* ret = &threads[0];
    for(size_t i = 1;i < DB_THREAD_COUNT;i++)
      if(threads[i].nsSpentOnLastFrame < ret->nsSpentOnLastFrame)
	ret = &threads[i];
    return ret;
  }

  DBEntity* Database::allocate(DBRecord::type_t type, size_t count, bool isHead) {
    DBThread* t = getLightestThread();//TODO something cleverer
    DBEntity* ret = free->pop();
    if(!ret) return NULL;
    ret->masterThread = t->dbId;
    t->addToSlice(ret);
    DBDelta delta;//set allocated flag
    delta.targetEntityId = ret->idx;
    delta.flagWriteValues = DBRecordFlag::allocated | (isHead ? DBRecordFlag::head_node : DBRecordFlag::none);
    delta.write_type = 1;
    delta.new_type = type;
    if(count > 1) {
      delta.flagWriteValues |= DBRecordFlag::has_next;
      delta.write_nextGlobalId = true;
      delta.new_nextGlobalId = allocate(type, count - 1, false)->idx;
    }
    delta.flagWriteMask = DBRecordFlag::all;
    write(&delta);
    return ret;
  }

  const struct entity_type* Database::getType(DBRecord::type_t t) {
    return &types[t];
  }

  void Database::deallocate(DBEntity* libre, DBRecord* state) {
    free->push(libre);
    threads[libre->idx].removeFromSlice(libre);
    libre->masterThread = ~0;
    {
      DBRecord temp;
      if(!state) {
	//TODO only read the part that we need
	read(getEntity(libre->idx), &temp);
	state = &temp;
      }
    }
    if((state->header.flags & DBRecordFlag::has_next) != 0) {
      deallocate(getEntity(state->header.nextGlobalId));
    }
    DBDelta delta;
    delta.targetEntityId = libre->idx;
    //set allocated flag
    delta.flagWriteMask = DBRecordFlag::all;
    delta.flagWriteValues = DBRecordFlag::none;
    write(&delta);
  }

  DBThread* Database::getCurrentThread() {
    return &threads[threadsByTid[DBThread::getCurrentTid()]];
  }

  void Database::write(DBDelta* src) {
    if(started) {
      src->frame = currentFrame;
      auto threadLog = &getCurrentThread()->transactions;
      if(!threadLog->push(src)) {
	Util::ScopeLock lock(&logMutex);
	size_t freeLog = log.freeSpace();
	if(freeLog < DBThread::TRANSACTION_COUNT)
	  applyLogTransactions(DBThread::TRANSACTION_COUNT - freeLog);
	DBDelta temp;
	while(threadLog->pop(&temp)) {
	  DBEntity* e = getEntity(temp.targetEntityId);
	  DBDelta* last = e->lastLog,
	    *newLast = log.push(&temp);
	  e->lastLog = newLast;
	  last->nextForEntity = newLast;
	}
	threadLog->push(src);//threadLog is now empty, no point in checking the result
      }
    } else {
      //this is for setting up the game start or menus
      src->applyTo(&data[src->targetEntityId]);
    }
  }

  bool Database::deltaIsInPast(DBDelta* t) {
    return t->frame < currentFrame;
  }

  size_t Database::applyLogTransactions(size_t minimumApplyRemaining) {
    size_t ret = 0;
    static const size_t MAX_BATCH_SIZE = 4*1024/sizeof(DBDelta);
    DBDelta batch[MAX_BATCH_SIZE];//4kb (max thread stack size (ulimit -s) is often 8kb
    do {
      size_t thisBatchSize = log.bulkPop(batch, MAX_BATCH_SIZE, deltaIsInPast_cb.get());
      for(size_t i = 0;i < thisBatchSize;i++) {
	DBDelta* delta = &batch[i];
	size_t id = delta->targetEntityId;
	DBEntity* dbe = &metadata[id];
	delta->applyTo(&data[id]);
	//TODO memory barrier?
	dbe->firstLog = delta->nextForEntity;
	if(!delta->nextForEntity)
	  dbe->lastLog = NULL;
      }
      if(minimumApplyRemaining <= thisBatchSize) {
	minimumApplyRemaining = 0;
      } else if(thisBatchSize < MAX_BATCH_SIZE) {//this means ran out of deltas that belong to previous frames to flush, yet still don't have enough room in the log; this is fatal
	//this is actually the best possible time to crash: we have just finished writing an entire frame to the save
	msync(data_raw, sizeof(DBRecord) * entityCount, MS_SYNC);//flush
	shuttingDown = true;
	exit(EXIT_FAILURE);
	while(1);//do not release mutex
      } else {
	minimumApplyRemaining -= thisBatchSize;
      }
      ret += thisBatchSize;
    } while(minimumApplyRemaining);
    return ret;
  }

  void Database::read(DBEntity* src, DBRecord* dst) {
    *dst = data[src->idx];
    DBDelta* delta = src->firstLog;
    while(delta && delta->frame < currentFrame) {
      delta->applyTo(dst);
      delta = delta->nextForEntity;
    }
  }

  DBEntity* Database::getEntityAfter(DBEntity* src) {
    size_t id = ~0;
    DBDelta* delta = src->firstLog;
    while(delta && delta->frame < currentFrame) {
      if(delta->write_nextGlobalId)
	id = delta->new_nextGlobalId;
      delta = delta->nextForEntity;
    }
    return id == ~0 ? NULL : getEntity(id);
  }

}
