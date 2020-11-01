#include "Internal.h"

#define WORK_NONE (std::numeric_limits<size_t>::max())

typedef struct {
  std::atomic<size_t> start;
  size_t len;
} workerData_t;

void initWorker(workerData_t* out) {
  out->len = 0;
  out->start.store(WORK_NONE);
}

static WITE::ThreadResource<workerData_t> workerData;
static std::vector<WITE::Database::Entry> entitiesWithUpdate;
size_t threadCount = 12;
uint8_t vertBuffer;
static size_t workerThreadCount = threadCount - 2, workunitIdx;//(how many threads? query number of cpu cores? user setting?)
static std::atomic_uint8_t masterState = 0;

size_t getThreadCount() {
  return threadCount;
}

uint8_t getVertBuffer() {
  return vertBuffer;
}

void enterWorker(void* unused) {
  WITE::Database::Entry target;
  size_t start, end;
  std::shared_ptr<workerData_t> sync = workerData.get();
  WITE::Database::typeHandles* handles = NULL;
  WITE::Database::type lastType = 0, type;
  while(true) {
    do {
      start = sync->start.load(std::memory_order_seq_cst);
      if(masterState.load(std::memory_order_relaxed) == 2) return;
      WITE::sleep(100);
    } while(start == WORK_NONE);
    end = sync->len + start;
    while(start < end) {
      target = entitiesWithUpdate[start];
      type = database->getEntryType(target);
      if(!handles || type != lastType) {
        handles = WITE::Database::getHandlesOfType(type);
        if(!handles || !handles->update)
          CRASH("Illegal entry type in typesWithUpdate, entry: %I64d of type %d, frame: %I64d\n", start, type, database->getCurrentFrame());
        //FIXME this is still happening sometimes, start: 0, type: 0, frame: 11, 273
        lastType = type;
      }
      handles->update->call(target);
      start++;
    }
    sync->start.store(WORK_NONE, std::memory_order_seq_cst);
  }
}

inline size_t dispatchWork() {//returns number of work units remaining
  size_t i, len, l, remainingUnits;
  typename decltype(workerData)::Tentry* threadlist;
  do {
    len = workerData.listAll(&threadlist);
  } while (!threadlist);
  for(i = 0;i < len;i++) {
    if(threadlist[i] && threadlist[i]->start.load(std::memory_order_seq_cst) == WORK_NONE) {
      remainingUnits = entitiesWithUpdate.size() - workunitIdx;
      l = glm::min<size_t>(remainingUnits, glm::max<size_t>(4, remainingUnits / (workerThreadCount*2)));
      if(l) {
	threadlist[i]->len = l;
	threadlist[i]->start.store(workunitIdx, std::memory_order_seq_cst);
	workunitIdx += l;
      }
    }
  }
  return entitiesWithUpdate.size() - workunitIdx;
}

extern void updateTime();
extern void initTime();

void enterMainLoop() {
  masterState = 1;
  size_t i;
  static std::atomic<int8_t> meshSemaphore;
  int8_t tempu8;
  initTime();
  meshSemaphore.store(0, std::memory_order_relaxed);
  WITE::Thread::spawnThread(WITE::Thread::threadEntry_t_F::make<void*>(&meshSemaphore, &Mesh::proceduralMeshLoop));
  auto workerEntry = WITE::Thread::threadEntry_t_F::make<void*>(NULL, &enterWorker);
  for(i = 0;i < workerThreadCount;i++) WITE::Thread::spawnThread(workerEntry);
  do {
    tempu8 = meshSemaphore.load(std::memory_order_consume);
  } while(!tempu8);
  vertBuffer = tempu8 - 1;
  meshSemaphore.store(0, std::memory_order_release);
  while(true) {
    TIME(database->advanceFrame(), 1, "advance frame (db wait): %llu\n");//no updates on frame 0.
    if(masterState.load(std::memory_order_relaxed) == 2) break;
    TIME(Window::renderAll(), 1, "Render: %llu\n");//Expensive. note: includes load of transforms of objects at renderFrame
    updateTime();
    entitiesWithUpdate.clear();
    workunitIdx = 0;
    TIME(database->getEntriesWithUpdate(&entitiesWithUpdate), 2, "entry setup: %llu\n");
    TIME(dispatchWork(), 2, "first dispatch: %llu\n");//TODO specify longer total jobs
    TIME(while(!Window::areRendersDone()) dispatchWork(), 1, "Wait for render: %llu\n");//TODO work units performed here give a metric for comparative load
    TIME(while(dispatchWork());, 1, "Wait for workers: %llu\n");
    TIME(Window::presentAll(), 1, "Present: %llu\n");//this may wait for vblank, if the fifo queue is full (which means we're 2+ frames ahead, 1 unseen in the queue and one cpu just completed
    tempu8 = meshSemaphore.load(std::memory_order_acquire);
    if(tempu8) {
      vertBuffer = tempu8 - 1;
      meshSemaphore.store(0, std::memory_order_release);
    }
  }
  meshSemaphore.store(-1, std::memory_order_release);
  while(meshSemaphore.load(std::memory_order_consume) != -2);
}

export_def void WITE::enterMainLoop() {
  ::enterMainLoop();
}

export_def void WITE::gracefulExit() {
  masterState = 2;
}
