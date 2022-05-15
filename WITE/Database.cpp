#include "DBDatabase.hpp"
#include "DBRecord.hpp"
#include "DBDelta.hpp"
#include "DBThread.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

namespace WITE::DB {

  Database::Database(struct entity_type* types, size_t typeCount) {
    DBThread* threads = reinterpret_cast<DBThread*>(malloc(sizeof(DbThread) * DB_THREAD_COUNT));
    if(!threads)
      CRASH_PREINIT("Faialed to create thread array");
    for(size_t i = 0;i < DB_THREAD_COUNT;i++)
      new(&threads[i])DBThread(this, i);
    this.threads = std::unique_ptr<DBThread>(threads);
    //TODO populate threadsByTid
    for(size_t i = 0;i < typeCount;i++) {
      this->types.insert({types[i]., types[i]});
    }
  }

  Database::Database(struct entity_type* types, size_t typeCount, size_t entityCount) :
    this(types, typeCount),
    entityCount(entityCount), free(entityCount) {
    DBEntity* entities = reinterpret_cast<DBEntity*>(malloc(sizeof(DBEntity) * entityCount));
    for(size_t i = 0;i < entityCount;i++)
      free.push(new(&entities[i])DBEntity(this, i));
    //no need to load anything when there's no file to load from
    this->metadata = std::unique_ptr(entities);
    data = reinterpret_cast<DBRecord*>(mmap(NULL, sizeof(DBRecord) * entityCount, PROT_READ | PROT_WRITE,
					    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_2MB, -1, 0));
  }

  Database::Database(struct entity_type* types, size_t typeCount, int backingStoreFd, size_t entityCount) :
    this(types, typeCount) {
    if(!entityCount) {
      struct stat stat;
      fstat(backingStoreFd, &stat);
      entityCount = stat.st_size / sizeof(DBRecord);
    }
    this->entityCount = entityCount;
    free = AtomicRollingQueue<DBEntity*>(entityCount);
    data = reinterpret_cast<DBRecord*>(mmap(NULL, sizeof(DBRecord) * entityCount, PROT_READ | PROT_WRITE,
					    MAP_SHARED | MAP_HUGETLB | MAP_HUGE_2MB | MAP_POPULATE, backingStoreFd, 0));
    DBEntity* entities = reinterpret_cast<DBEntity*>(malloc(sizeof(DBEntity) * entityCount));
    for(size_t i = 0;i < entityCount;i++) {
      DBEntity* dbe = new(&entities[i])DBEntity(this, i);
      if(data[i].header.flags & DBRecord::flag_t::allocated) {
	size_t tid = i % DB_THREAD_COUNT;
	dbe->masterThread = tid;
	if(getType
	threads[tid].addToSlice(dbe);
      } else {
	free.push(dbe);
      }
    }
    this->metadata = std::unique_ptr(entities);
  }

  //TODO destructor, close all db threads by setting semaphore to state_exiting

  DBThread* getLightestThread() {
    DBThread* ret = &threads[0];
    for(size_t i = 1;i < DB_THREAD_COUNT;i++)
      if(threads[i].nsSpentOnLastFrame < ret->nsSpentOnLastFrame)
	ret = &threads[i];
    return ret;
  }

  DBEntity* Database::allocate(DBRecord::type_t type, size_t count, bool isHead) {
    DBThread* t = getLightestThread();//TODO something cleverer
    DBEntity* ret = free.pop();
    if(!ret) return NULL;
    ret->masterThread = t->dbId;
    t->addToSlice(ret);
    DBDelta delta;//set allocated flag
    delta.targetEntityId = ret->idx;
    delta.flagWriteValues = DBRecord::allocated | (isHead ? DBRecord::head_node : 0);
    delta.write_type = 1;
    delta.new_type = type;
    if(count > 1) {
      delta.flagWriteValues |= DBRecord::has_next;
      delta.write_nextGlobalId = true;
      delta.new_nextGlobalId = allocate(type, count - 1, false)->idx;
    }
    delta.flagWriteMask = DBRecord::allocated | DBRecord::head_node | DBRecord::has_next;
    write(delta);
    return ret;
  }

  const struct entity_type* Database::getType(DBRecord::type_t t) {
    return &types[t];
  }

  void Database::deallocate(DBEntity* libre, DBRecord* state) {
    free.push(libre);
    threads[libre->idx].removeFromSlice(libre);
    libre->masterThread = ~0;
    union {
      DBRecord temp;
      DBDelta delta;
    }
    if(!state) {
      //TODO only read the part that we need
      read(getEntity(libre->idx), &temp);
      state = &temp;
    }
    if(state->header.flags & DBRecord::has_next) {
      deallocate(getEntity(state->header.nextGlobalId));
    }
    delta = DBDelta();
    delta.targetEntityId = libre->idx;
    //set allocated flag
    delta.flagWriteMask = ~0;
    delta.flagWriteValues = 0;
    write(delta);
  }

  void Database::write(DBDelta* src) {
    if(started) {
      delta->frame = curentFrame;
      auto threadLog = &DBThread.getCurrentThread()->transactions;
      if(!threadLog->push(src)) {
	ScopeLock lock(&logMutex);
	size_t freeLog = log.freeSpace();
	if(freeLog < DBThread::TRANSACTION_COUNT)
	  applyLogTransactions(DBThread::TRANSACTION_COUNT - freeLog);
	DBDelta temp;
	while(threadLog->pop(&temp)) {
	  DBEntity* e = getEntity(temp.targetEntityId);
	  DBDelta* last = e->last,
	    *newLast = log.push(temp);
	  e->last = newLast;
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

  static Database::Callback_t<bool, DBDelta*> deltaIsInPast_cb =
    Callback_Factory<bool, DBDelta*>::make(&Database::deltaIsInPast);

  void Database::applyLogTransactions(size_t minimumApplyRemaining) {
    static const size_t MAX_BATCH_SIZE = 4*1024/sizeof(DBDelta);
    DBDelta batch[MAX_BATCH_SIZE];//4kb (max thread stack size (ulimit -s) is often 8kb
    do {
      size_t thisBatchSize = log.bulkPop(&batch, MAX_BATCH_SIZE, &deltaIsInPast_cb);
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
	msync(reinterpret_cast<void*>(data), sizeof(DBRecord) * entityCount, MS_SYNC);//flush
	shuttingDown = true;
	exit(EXIT_FAILURE);
	while(1);//do not release mutex
      }
    } while(minimumApplyRemaining);
  }

  void Database::read(DBEntity* src, DBRecord* dst) {
    dst* = data[src->idx];
    DBDelta* delta = src->firstLog;
    while(delta && delta->frame < currentFrame) {
      delta->apply(dst);
      delta = delta->nextForEntity;
    }
  }

  DBEntity* Database::getEntityAfter(DBEntity* src) {
    size_t id = ~0;
    DBDelta* delta = src->firstLog;
    while(delta && delta->frame < currentFrame) {
      if(delta->write_nextGlobalId)
	id = delta->new_next_GlobalId;
      delta = delta->nextForEntity;
    }
    return id == ~0 ? NULL : getEntity(id);
  }

}





// #include "Database.hpp"
// #include "DBEntity.hpp"
// #include "DBThread.hpp"

// namespace WITE::DB {

//   Database::Database(int threadCount, FILE* backingStore) : this(threadCount, mmap(TODO), 0) {}

//   Database::Database(int threadCount, void* backingStore, size_t backingStoreSize) :
//     metadata(std::make_unique<DBEntity[]>(backingStoreSize)),
//     shuttingDown(false),
//     threadCount(threadCount) {
//     DBThread* threads = new DBThread[threadCount];
//     for(size_t i = 0;i < threadCount;i++) {
//       new(&threads[i])DBThread(this, i);
//     }
//     this.threads = new std::unique_ptr<DBThread[]>(threads);
//   }

//   Database::anyThreadIs(DBThread::semaphoreState state) {
//     for(size_t i = 0;i < threadCount;i++)
//       if(threads[i].semaphore == state)
// 	return true;
//     return false;
//   }

//   void mainLoop() {
//     for(size_t i = 0;i < threadCount;i++)
//       threads[i].semaphore = startup;
//     size_t entityInCommitSweep = 0;
//     DBEntity* tempE;
//     while(!shuttingDown) {
//       // gathering: each thread calls update for each entity it owns (generating transactions)
//       for(size_t i = 0;i < threadCount && !shuttingDown;i++)
// 	threads[i].semaphore = gathering;
//       //     while those do that, this thread applies logs from previous frames to the backing store
//       while(anyThreadIs(gathering)) {
// 	tempE = getEntity(id);
// 	if(tempE->pendingTransactions) {
// 	  [[likely]]
// 	  //TODO apply backlog
// 	}
// 	if(++entityInCommitSweep >= entityCount) [[unlikely]] entityInCommitSweep = 0;
//       }
//       for(size_t i = 0;i < threadCount && !shuttingDown;i++)
// 	while(threads[i].semaphore != gathered);
//       // mapping: this thread builds a conflict tree forest of transactions and assigns each transaction tree to a worker thread
//       //TODO
//       // resolving: each thread calls the conflict resolution on each transaction tree it has been assigned, writing single-object deltas to those objects' own log spaces
//       for(size_t i = 0;i < threadCount && !shuttingDown;i++)
// 	threads[i].semaphore = gathering;
//       //    while they do that, this thread rethinks which thread owns what DbEntities (takes affect on the next gathering step)
//       //    also expands the backing file if it exists and space is running low
//       //TODO
//       for(size_t i = 0;i < threadCount && !shuttingDown;i++)
// 	while(threads[i].semaphore != gathered);
//     }
//     for(size_t i = 0;i < threadCount;i++)
//       threads[i].semaphore = exiting;
//   }

//   void write(DBOpenTransaction* transaction) {
//     DBThread* thisThread = DBThread::getCurrentThread();
//     if(!thisThread)
//       [[unlikely]]
// 	throw std::runtime_exception("Transaction cannot be saved from non-worker thread. This is a bug. Transactions should only be created via update.");
//     //TODO
//   }

// }
