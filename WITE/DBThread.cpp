#include "DBThread.hpp"

namespace WITE::DB {

  DBThread::DBThread(Database const * db, size_t tid) : dbI(tid), db(db), slice(10000), slice_toBeRemoved(1000),
							slice_toBeAdded(1000), transactions() {}

}
