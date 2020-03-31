#include "stdafx.h"
#includ "Globals.h"

#define WORK_NONE (-1)

typedef struct workerData_t {
  std::atomic<size_t> start;
  size_t len;
};

void initWorker(workerData_t* out) {
  out->start.store(WORK_NONE, std::memory_order_relaxed);
  out->len = 0;
}

static ThreadedResource<workerData_t> workerData;
static std::vector<Database::Entry> entitiesWithUpdate(1024*1024);
static size_t threadCount = 12, workerThreadCount = threadCount - 2, workunitIdx;//(how many? query number of cpu cores? user setting?)

/*export*/ void WITE_INIT(const char* gameName);
