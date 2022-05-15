#include "RecyclingQueue.hpp" //template class

namespace WITE::DB {

  class Database;
  class DBDelta;
  class DBEntity;

  class DBThread : public Thread {
  private:
    static constexpr size_t TRANSACTION_COUNT = 128*1024*1024 / sizeof(DBDelta);//~700k
    enum semaphoreState : uint8_t { state_initial, state_updating, state_updated, state_maintaining, state_exiting, state_exploded };
    std::atomic<semaphoreState> semaphore = state_initial;
    size_t dbId;
    Database* db;
    DBThread(const DBThread*) = delete;
    DBThread() = delete;
    DBThread(Database const *, size_t tid);//TODO set stack size? See ulimit -s
    //slice only contains head entities of types that have a registered update cb, to optimize for updates
    List<DBEntity*> slice, slice_toBeRemoved, slice_toBeAdded;
    RollingQueue<DBDelta, TRANSACTION_COUNT> transactions;
    friend class Database;
    void workLoop();
    void start();//called by db
    void addToSlice(DBEntity*);
    void removeFromSlice(DBEntity*);
    uint64_t nsSpentOnLastFrame;
  public:
    static DBThread* getCurrentThread();
  }

}
