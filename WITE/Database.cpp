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
#include "DBRecordFlag.hpp"

#define PAGEPOW 23
#define MAPTHRESH (2 << PAGEPOW)

namespace WITE::DB {

  Database::Database(struct entity_type* types, size_t typeCount, size_t entityCount, int backingStoreFd) {
    //entityCount param will yield to size of file if backing store is given and exists
    deltaIsInPast_cb = deltaIsInPast_cb_t_F::make(this, &Database::deltaIsInPast);
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
    size_t mmapsize = sizeof(DBRecord) * entityCount;
    // if(mmapsize > MAPTHRESH)
    //   mmapFlags |= MAP_HUGETLB;// | (PAGEPOW << MAP_HUGE_SHIFT);
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
    Util::ScopeLock lock(&free_mutex);
    for(size_t i = 0;i < entityCount;i++) {
      //TODO progress report callback for loading bar?
      DBEntity* dbe = new(&entities[i])DBEntity(this, i);
      if((data[i].header.flags & DBRecordFlag::allocated) != 0) {
	size_t tid = i % DB_THREAD_COUNT;
	auto type = &this->types[cv_remove(data[i].header.type)];
	if(type->onSpinUp)
	  type->onSpinUp(&entities[i]);
	if((data[i].header.flags & DBRecordFlag::head_node) != 0) {
	  threads[tid].addToSlice(dbe, type->typeId);
	}
      } else {
	free.push(dbe);
      }
    }
    this->metadata = entities;
  }

  Database::~Database() {
    stop();
    DBDelta transaction;
    for(size_t i = 0;i < DB_THREAD_COUNT;i++) {
      while(threads[i].transactionPool.popIf(&transaction, deltaIsInPast_cb))
	transaction.applyTo(&data[i]);
      threads[i].~DBThread();
    }
    ::free(threads);
    ::free(metadata);
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
    for(size_t i = 0;i < DB_THREAD_COUNT;i++) {
      bool good = threads[i].setState(from, to);
      ASSERT_TRAP(good, "thread state desync!");
    }
    for(size_t i = 0;i < DB_THREAD_COUNT;i++) {
      while(true) {
	auto state = threads[i].semaphore.load(std::memory_order_consume);
	if(state == waitFor) break;
	if(state != to) CRASH("Thread state mismatch! signalThreads(from, to, waitFor)", (int)from, ", ", (int)to, ", ",
			      (int)waitFor, ", ", " but thread ", i, " is in state ", (int)state);
	WITE::Platform::Thread::sleepShort();
      }
    }
  }

  Database* Database::liveDb = NULL;

  void Database::start() {
    liveDb = this;
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
    DBEntity* ret;
    {
      Util::ScopeLock lock(&free_mutex);
      if(free.empty()) return NULL;
      ret = free.front();
      free.pop();
    }
    if(started && ret->lastWrittenFrame == currentFrame)//the chosen entity was deallocated on this frame
      return NULL;
    //LOG("Allocated:  ", ret->getId(), " on frame ", currentFrame);
#ifdef DEBUG
    DBRecord::header_t header;
    readHeader(ret, &header);
    ASSERT_TRAP(!(header.flags >> DBRecordFlag::allocated), "Allocated entity found in free queue! ent: ", ret->getId(), ", frame: ", currentFrame, ", last log: ", (ret->log.peekLast() ? ret->log.peekLast()->frame : 0), ", last write: ", ret->lastWrittenFrame);
#ifdef DEBUG_THREAD_SLICES
    ret->lastAllocatedFrame = currentFrame;
#endif
#endif
    ASSERT_TRAP(std::less()(metadata-1, ret) && std::less()(ret, metadata + entityCount), "Illegal entity returned from free");
    ASSERT_TRAP(ret->masterThread == ~0, "Duplicate entity allocation.");
    if(isHead) {
      t->addToSlice(ret, type);
    } else {
      ret->masterThread = ~0;
    }
    return ret;
  }

  const struct entity_type* Database::getType(DBRecord::type_t t) {
    return &types[t];
  }

  void Database::deallocate(DBEntity* libre, DBRecord::header_t* state) {
    ASSERT_TRAP(std::less()(metadata-1, libre) && std::less()(libre, metadata + entityCount), "Illegal entity fed to free");
    // LOG("Deallocated:", libre->getId(), " on frame ", currentFrame);
    ASSERT_TRAP(libre->masterThread < DB_THREAD_COUNT, "attempting to free entity with no master");
    threads[libre->masterThread].removeFromSlice(libre);
    DBRecord::header_t stateHeader = {};
    if(!state) {
      //TODO only read the part that we need
      readHeader(getEntity(libre->idx), &stateHeader);
      state = &stateHeader;
    }
    if((state->flags & DBRecordFlag::has_next) != 0) {
      deallocate(getEntity(state->nextGlobalId));
    }
#ifdef DEBUG_THREAD_SLICES
    libre->lastDeallocatedFrame = currentFrame;
#endif
    auto type = &types[state->type];
    if(type->onSpinDown)
      type->onSpinDown(libre);
    if(type->onDeallocate)
      type->onDeallocate(libre);
    DBDelta delta;
    delta.clear();
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
      ASSERT_TRAP((void*)this == (void*)liveDb, "I am a teapot");
      ASSERT_TRAP(getEntity(src->targetEntityId)->masterThread == getCurrentThread()->dbId,
		  "Write to entity from wrong thread!");
      src->frame = metadata[src->targetEntityId].lastWrittenFrame = currentFrame;
      src->integrityCheck(this);
      // LOG("Writing delta: ", src, *src);
      getEntity(src->targetEntityId)->log.push(getCurrentThread()->transactionPool.push(src));
    } else {
      // LOG("Pre-start writing delta: ", src, *src);
      //this is for setting up the game start or menus
      src->frame = 0;
      src->integrityCheck(this);
      src->applyTo(&data[src->targetEntityId]);
    }
  }

  void Database::idle(DBThread* thread) {
    for(size_t i = 0;i < 100;i++)
      if(!applyLogTransactions(thread))
	break;
    //TODO other stuff?
  };

  bool Database::deltaIsInPast(const DBDelta* t) {
    return t->frame < currentFrame;
  }

  size_t Database::applyLogTransactions(DBThread* thread) {
    size_t ret = 0;
    DBDelta transaction;
    //TODO more dynamic batch size, and larger batches
    if(thread->transactionPool.popIf(&transaction, deltaIsInPast_cb)) {
      size_t id = transaction.targetEntityId;
      ASSERT_TRAP(transaction.frame != currentFrame, "Attempted to apply log on its origin frame!");
      DBEntity* dbe = &metadata[id];
      transaction.applyTo(&data[id]);
      DBDelta* temp = dbe->log.pop();
      ASSERT_TRAP(*temp == transaction, "Logs applied out of order!");
      if((transaction.flagWriteMask >> DBRecordFlag::allocated) &&
	 !(transaction.flagWriteValues >> DBRecordFlag::allocated)) {
	//on deallocate
	dbe->masterThread = ~0;
	Util::ScopeLock lock(&free_mutex);
	free.push(dbe);
      }
      ret++;
    };
    return ret;
  }

  void Database::read(DBEntity* src, DBRecord* dst) {
    auto iter = src->log.begin();
    *dst = *cv_remove(&data[src->idx]);
    while(iter && iter->frame < currentFrame) {
      iter->applyTo(dst);
      ASSERT_TRAP(iter->targetEntityId == src->getId(), "Log stack collission, frame: ", currentFrame);
      // ASSERT_TRAP(iter->nextForEntity || iter == src->log.peekLastDirty(), "Desync at end of log");
      iter++;
    }
  }

  void Database::readHeader(DBEntity* e, DBRecord::header_t* out, bool readNext, bool readType, bool readFlags) {
    auto delta = e->log.begin();
    *out = *cv_remove(&data[e->idx].header);
    while(delta && delta->frame < currentFrame) {
      ASSERT_TRAP(delta->targetEntityId == e->getId(), "Log stack collission, frame: ", currentFrame);
      if(readNext && delta->write_nextGlobalId)
	out->nextGlobalId = delta->new_nextGlobalId;
      if(readType && delta->write_type)
	out->type = delta->new_type;
      if(readFlags && delta->flagWriteMask != 0)
	out->flags ^= delta->flagWriteMask & (out->flags ^ delta->flagWriteValues);
      // ASSERT_TRAP(delta->nextForEntity || delta == e->log.peekLastDirty(), "Desync at end of log");
      delta++;
    }
  }

  DBEntity* Database::getEntityAfter(DBEntity* src) {
    DBRecord::header_t header;
    readHeader(src, &header, true, false, true);
    return (header.flags & DBRecordFlag::has_next) != 0 ? getEntity(header.nextGlobalId) : NULL;
  }

  Database::threadTypeSearchIterator::fetcher_cb_return
  typeMembersFromThread(DBRecord::type_t t,
			Database::threadTypeSearchIterator::fetcher_cb_param thread) {
    auto ret = thread->getSliceMembersOfType(t);
    return ret;
  }

  Database::threadTypeSearchIterator Database::getByType(DBRecord::type_t t) const {
    auto l2_cb = threadTypeSearchIterator::fetcher_cb_F::make<DBRecord::type_t>(t, &typeMembersFromThread);
    return threadTypeSearchIterator(Collections::IteratorWrapper(threads, DB_THREAD_COUNT), l2_cb);
  };

  DBEntity* Database::getEntity(size_t id) {
    ASSERT_TRAP(id < entityCount, "insane entity id: ", id);
    return &metadata[id];
  };

  size_t Database::getEntityCount() {
    return entityCount;
  };

}
