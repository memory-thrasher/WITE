#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include "Thread.hpp"
#include "Database.hpp"
#include "DBEntity.hpp"
#include "DBRecord.hpp"
#include "DBDelta.hpp"
#include "DBThread.hpp"
#include "DEBUG.hpp"

#define PAGEPOW 23
#define MAPTHRESH (2 << PAGEPOW)

namespace WITE::DB {

  Database::Database(struct entity_type* types, size_t typeCount, size_t entityCount, int backingStoreFd) {
    //entityCount param will yield to size of file if backing store is given and exists
    deltaIsInPast_cb = Util::CallbackFactory<bool, DBDelta*>::make_unique(this, &Database::deltaIsInPast);
    threads = reinterpret_cast<DBThread*>(malloc(sizeof(DBThread) * DB_THREAD_COUNT));
    if(!threads)
      CRASH_PREINIT("Failed to create thread array");
    for(size_t i = 0;i < DB_THREAD_COUNT;i++)
      new(&threads[i])DBThread(this, i);
    for(size_t i = 0;i < typeCount;i++) {
      this->types.insert({types[i].typeId, types[i]});
    }
    int mmapFlags = 0;
    if(backingStoreFd >= 0) {
      struct stat stat;
      fstat(backingStoreFd, &stat);
      size_t entityCountFromFile = stat.st_size / sizeof(DBRecord);
      if(entityCountFromFile > entityCount)
	entityCount = entityCountFromFile;
      // else
	//TODO grow file
      mmapFlags |= MAP_SHARED | MAP_POPULATE;
    } else { //anonymous db
      mmapFlags |= MAP_PRIVATE | MAP_ANONYMOUS;
    }
    this->entityCount = entityCount;
    free = std::make_unique<Collections::AtomicRollingQueue<DBEntity*>>(entityCount);
    size_t mmapsize = sizeof(DBRecord) * entityCount;
    if(mmapsize > MAPTHRESH)
      mmapFlags |= MAP_HUGETLB | (PAGEPOW << MAP_HUGE_SHIFT);
    data_raw = mmap(NULL, mmapsize, PROT_READ | PROT_WRITE, mmapFlags, backingStoreFd, 0);
    if(data_raw == MAP_FAILED) {
      int e = errno;
      if(e == ENOMEM) { //data rlimit might be to blame. Try to increase it.
	WARN("Attempting to increase data rlimit because mmap failed with ENOMEM");
	struct rlimit rlim;
	if(getrlimit(RLIMIT_DATA, &rlim)) CRASH_PREINIT("Failed to fetch data rlimit");
	WARN("Data rlimt old: ", rlim.rlim_cur);
	rlim.rlim_cur = Util::max(rlim.rlim_cur + mmapsize, rlim.rlim_max);
	WARN("Data rlimt new: ", rlim.rlim_cur);
	if(rlim.rlim_cur < mmapsize) CRASH_PREINIT("Data rlimit hard limit too low");
	if(setrlimit(RLIMIT_DATA, &rlim)) CRASH_PREINIT("Failed to set data rlimit");
	data_raw = mmap(NULL, mmapsize, PROT_READ | PROT_WRITE, mmapFlags, backingStoreFd, 0);
	e = errno;
      }
      if(data_raw == MAP_FAILED)
	CRASH_PREINIT("Failed to create memory map with size ", sizeof(DBRecord) * entityCount,
		      " flags ", mmapFlags, " and errno ", e);
    }
    DBEntity* entities = reinterpret_cast<DBEntity*>(malloc(sizeof(DBEntity) * entityCount));
    for(size_t i = 0;i < entityCount;i++) {
      //TODO progress report callback for loading bar?
      DBEntity* dbe = new(&entities[i])DBEntity(this, i);
      if((data[i].header.flags & DBRecordFlag::allocated) != 0) {
	size_t tid = i % DB_THREAD_COUNT;
	auto type = &this->types[data[i].header.type];
	if(type->onSpinUp)
	  type->onSpinUp(&entities[i]);
	if((data[i].header.flags & DBRecordFlag::head_node) != 0) {
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

  void Database::signalThreads(DBThread::semaphoreState from, DBThread::semaphoreState to, DBThread::semaphoreState waitFor) {
    for(size_t i = 0;i < DB_THREAD_COUNT;i++)
      threads[i].setState(from, to);
    for(size_t i = 0;i < DB_THREAD_COUNT;i++) {
      while(true) {
	auto state = threads[i].semaphore.load(std::memory_order_consume);
	if(state == waitFor) break;
	if(state != to) CRASH("Thread state mismatch! signalThreads(from, to, waitFor)", from, to, waitFor,
			      " but thread ", i, "is in state ", state);
	WITE::Platform::Thread::sleepShort();
      }
    }
  }

  void Database::start() {
    Platform::Thread::init();
    started = true;
    for(size_t i = 0;i < DB_THREAD_COUNT;i++) {
      threads[i].start();
      threadsByTid.insert({ threads[i].getTid(), i });
    }
    signalThreads(DBThread::state_ready, DBThread::state_updating, DBThread::state_updated);
    while(true) {
      if(anyThreadBroke()) {
	stop();
	break;
      }
      signalThreads(DBThread::state_updated, DBThread::state_maintaining, DBThread::state_maintained);
      if(anyThreadBroke() || shuttingDown) {
	stop();
	break;
      }
      currentFrame++;
      signalThreads(DBThread::state_maintained, DBThread::state_updating, DBThread::state_updated);
    }
    for(auto& type : types)
      if(type.second.onSpinDown)
	for(DBEntity& e : getByType(type.first))
	  type.second.onSpinDown(&e);
  }

  void Database::shutdown() {
    shuttingDown = true;
  }

  void Database::stop() {
    shuttingDown = true;
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
    ASSERT_TRAP(std::less()(metadata-1, ret) && std::less()(ret, metadata + entityCount), "Illegal entity returned from free");
    if(isHead) {
      ret->masterThread = t->dbId;
      t->addToSlice(ret);
    } else {
      ret->masterThread = ~0;
    }
    DBDelta delta;//set allocated flag
    delta.targetEntityId = ret->idx;
    delta.flagWriteValues = DBRecordFlag::allocated | (isHead ? DBRecordFlag::head_node : DBRecordFlag::none);
    delta.write_type = true;
    delta.new_type = type;
    delta.write_nextGlobalId = true;
    if(count > 1) {
      delta.flagWriteValues |= DBRecordFlag::has_next;
      delta.new_nextGlobalId = allocate(type, count - 1, false)->idx;
    }
    delta.flagWriteMask = DBRecordFlag::all;
    delta.len = delta.dstStart = 0;
    delta.frame = 0;
    write(&delta);
    auto typeR = &types[type];
    if(typeR->onAllocate)
      typeR->onAllocate(ret);
    if(typeR->onSpinUp)
      typeR->onSpinUp(ret);
    return ret;
  }

  const struct entity_type* Database::getType(DBRecord::type_t t) {
    return &types[t];
  }

  void Database::deallocate(DBEntity* libre, DBRecord* state) {
    ASSERT_TRAP(std::less()(metadata-1, libre) && std::less()(libre, metadata + entityCount), "Illegal entity fed to free");
    free->push(libre);
    threads[libre->masterThread].removeFromSlice(libre);
    libre->masterThread = ~0;
    DBRecord temp;
    if(!state) {
      //TODO only read the part that we need
      read(getEntity(libre->idx), &temp);
      state = &temp;
    }
    if((state->header.flags & DBRecordFlag::has_next) != 0) {
      deallocate(getEntity(state->header.nextGlobalId));
    }
    auto type = &types[state->header.type];
    if(type->onSpinDown)
      type->onSpinDown(libre);
    if(type->onDeallocate)
      type->onDeallocate(libre);
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
    ASSERT_TRAP(src != NULL & src->targetEntityId < entityCount, "Illegal delta");
    if(started) {
      src->frame = currentFrame;
      auto threadLog = &getCurrentThread()->transactions;
      if(!threadLog->push(src)) {
	flushTransactions(threadLog);
	threadLog->push(src);//threadLog is now empty, no point in checking the result
      }
    } else {
      //this is for setting up the game start or menus
      src->applyTo(&data[src->targetEntityId]);
    }
  }

  void Database::flushTransactions(decltype(DBThread::transactions)* threadLog) {
    Util::ScopeLock lock(&logMutex);
    size_t freeLog = log.freeSpace();
    if(freeLog < DBThread::TRANSACTION_COUNT)
      applyLogTransactions(DBThread::TRANSACTION_COUNT - freeLog);
    DBDelta temp;
    while(threadLog->pop(&temp)) {
      DBEntity* e = getEntity(temp.targetEntityId);
      DBDelta* last = e->lastLog,
	*newLast = log.push(&temp);
      ASSERT_TRAP(newLast->nextForEntity == NULL, "New last log for entity has baggage");
      e->lastLog = newLast;
      if(last)
	last->nextForEntity = newLast;
      else
	e->firstLog = newLast;
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
	ERROR("Exiting due to insufficient transactional log space.");
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
