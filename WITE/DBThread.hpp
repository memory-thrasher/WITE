#include "RecyclingQueue.hpp" //template class
#include "SyncLock.hpp"

namespace WITE::DB::Platform { class Thread; }

namespace WITE::DB {

  class Database;
  class DBDelta;
  class DBEntity;

  class DBThread {
  private:
    typedef WITE::DB::Platform::Thread PThread;
    static constexpr size_t TRANSACTION_COUNT = 128*1024*1024 / sizeof(DBDelta);//~700k
    enum semaphoreState : uint8_t { state_initial, state_updating, state_updated, state_maintaining,
				    state_frameSync, state_exiting, state_exploded, state_exited };
    std::atomic<semaphoreState> semaphore = state_initial;
    size_t dbId;
    Database* db;
    PThread* thread;
    DBThread(const DBThread*) = delete;
    DBThread() = delete;
    DBThread(Database const *, size_t tid);//TODO set stack size? See ulimit -s
    //slice only contains head entities of types that have a registered update cb, to optimize for updates
    List<DBEntity*> slice, slice_toBeRemoved, slice_toBeAdded;
    SyncLock sliceAlterationPoolMutex;
    RollingQueue<DBDelta, TRANSACTION_COUNT> transactions;
    uint64_t nsSpentOnLastFrame;
    friend class Database;
    void workLoop();
    void start();//called by db
    void addToSlice(DBEntity*);
    void removeFromSlice(DBEntity*);
    bool setState(semaphoreState desired);
    bool waitForState(semaphoreState desired);
    void join();
    uint32_t getTid();
  public:
    static uint32_t getCurrentTid();
  }

}
