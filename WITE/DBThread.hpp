#pragma once

#include <map>

#include "IteratorWrapper.hpp"
#include "SyncLock.hpp"
#include "DBDelta.hpp"
#include "DBEntity.hpp"
#include "RollingQueue.hpp"
#include "LinkedList.hpp"

namespace WITE::Platform { class Thread; }

namespace WITE::DB {

  class Database;
  class DBEntity;

  class DBThread {
  private:
    typedef class WITE::Platform::Thread PThread;
    static constexpr size_t TRANSACTION_COUNT = 128*1024*1024 / sizeof(DBDelta);//671088
    enum semaphoreState : uint8_t { state_initial, state_ready, state_updating, state_updated, state_maintaining,
				    state_maintained, state_exiting, state_exploded, state_exited };
    std::atomic<semaphoreState> semaphore = state_initial;
    size_t dbId;
    Database* db;
    PThread* thread;
    DBThread(const DBThread&) = delete;
    DBThread() = delete;
    DBThread(Database* const, size_t tid);//TODO set stack size? See ulimit -s
    //slice is separated by if the entity type has a registered update cb, to optimize for updates
    std::vector<DBEntity*> slice_withUpdates, slice_withoutUpdates, slice_toBeRemoved;
    std::vector<std::pair<DBEntity*, DBRecord::type_t>> slice_toBeAdded;
    std::vector<DBRecord::type_t> temp_uniqTypes;
    std::map<DBRecord::type_t, Collections::LinkedList<DBEntity, &DBEntity::nextOfTypeInThread>> typeIndex;
    Util::SyncLock sliceAlterationPoolMutex;
    Collections::RollingQueue<DBDelta, TRANSACTION_COUNT> transactions;
    uint64_t nsSpentOnLastFrame;
    friend class Database;
    void workLoop();
    void start();//called by db
    void addToSlice(DBEntity*, DBRecord::type_t type);
    void removeFromSlice(DBEntity*);
    bool setState(semaphoreState old, semaphoreState desired);
    bool waitForState(semaphoreState desired);
    bool waitForState(semaphoreState old, semaphoreState desired);
    void join();
  public:
    const auto getSliceMembersOfType(DBRecord::type_t t) const {
      //Collections::IteratorWrapper<typeIndex::mapped_type::iterator>
      return typeIndex.contains(t) ? Collections::IteratorWrapper<decltype(typeIndex)::mapped_type::iterator>(typeIndex.at(t)) :
	Collections::IteratorWrapper<decltype(typeIndex)::mapped_type::iterator>();//empty iterator
    };
    static uint32_t getCurrentTid();
    inline uint32_t getTid() { return thread->getTid(); };
    inline uint32_t getDbid() { return dbId; };
  };

}
