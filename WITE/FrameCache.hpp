#pragma once

#include "Database.hpp"
#include "Callback.hpp"
#include "SyncLock.hpp"

namespace WITE::Transient {

  template<class T> class FrameCache {
  public:
    typedef DB::Database Database;
    typedefCB(fetcher_t, T, Database*);
    typedefCB(cleaner_t, void, T);

  private:
    Database* db;
    bool allowParallelFetch;
    fetcher_t fetch;
    cleaner_t clean;
    std::atomic_uint64_t cachedAt;
    T cachedData;
    SyncLock mutex;//only used if !allowParallelFetch
  public:

    FrameCache(Database* db, fetcher_t f, bool parallel, cleaner_t c) :
      db(db), allowParallelFetch(parallel), fetch(f), clean(c), cachedAt(db->getFrame()) {
      ASSERT_TRAP(!c || !parallel, "Cleaner not allowed with parallel fetch");
    };

    ~FrameCache() {
      if(clean) clean(cachedData);
    }

    FrameCache(FrameCache&) = delete;
    T get() {
      auto frame = db->getFrame();
      if(frame == cachedAt.load(std::memory_order_relaxed))
	return cachedData;
      if(allowParallelFetch) {
	cachedData = fetch(db);
	cachedAt.store(frame);
      } else {
	ScopeLock lock(&mutex);
	if(frame != cachedAt.load(std::memory_order_relaxed)) {
	  if(clean) clean(cachedData);
	  cachedData = fetch(db);
	  cachedAt.store(frame);
	}
      }
      return cachedData;
    }

  };

}

