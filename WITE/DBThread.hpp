#include "RecyclingQueue.hpp" //template class

namespace WITE::DB {

  class Database;
  class DBDelta;
  class DBEntity;

  class DBThread : public Thread {
  private:
    static constexpr size_t TRANSACTION_COUNT = 128*1024*1024 / sizeof(DBDelta);//~800k
    //enum semaphoreState : uint8_t { initial, startup, gathering, gathered, resolving, resolved, exiting, exploded };
    //std::atomic<semaphoreState> semaphore;
    size_t dbId;
    Database* db;
    DBThread(const DBThread*) = delete;
    DBThread() = delete;
    DBThread(Database const *, size_t tid);//TODO set stack size? See ulimit -s
    List<DBEntity*> slice, slice_toBeRemoved, slice_toBeAdded;
    RollingQueue<DBDelta, TRANSACTION_COUNT> transactions;
    friend class Database;
    void workLoop();
    void start();//called by db
    void addToSlice(DBEntity*);
    void removeFromSlice(DBEntity*);
    time_t timeSpentOnLastFrame;//TODO time_t, need better than ms
  public:
    static DBThread* getCurrentThread();
  }

}
