#include "stdafx.h"
#include "Globals.h"
#include "Database.h"

#define WORK_NONE (-1)

typedef struct {
  std::atomic<size_t> start;
  size_t len;
} workerData_t;

void initWorker(workerData_t* out) {
  out->start.store(WORK_NONE, std::memory_order_relaxed);
  out->len = 0;
}

static WITE::ThreadResource<workerData_t> workerData;
static std::vector<WITE::Database::Entry> entitiesWithUpdate(1024*1024);
static size_t threadCount = 12, workerThreadCount = threadCount - 2, workunitIdx;//(how many? query number of cpu cores? user setting?)

export_dec void WITE_INIT(const char* gameName);
