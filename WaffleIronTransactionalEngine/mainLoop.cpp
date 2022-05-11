#include "Internal.h"

#define WORK_NONE -1
#define WORK_FRAME_END -2
#define MASTER_STATE_QUIT 2

typedef struct {
  std::atomic<ssize_t> start;
  ssize_t len;
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
  ssize_t start, end;
  auto sync = workerData.get();
  WITE::Database::typeHandles* handles = NULL;
  WITE::Database::type lastType = 0, type;
  while(true) {
    do {
      start = sync->start.load(std::memory_order_seq_cst);
      if(masterState.load(std::memory_order_relaxed) == MASTER_STATE_QUIT) return;
      WITE::sleep(100);
    } while(start == WORK_NONE);
    switch(start) {
    default:
      end = sync->len + start;
      while(start < end) {
	target = entitiesWithUpdate[start];
	type = database->getEntryType(target);
	if(!handles || type != lastType) {
	  handles = WITE::Database::getHandlesOfType(type);
	  if(!handles || !handles->update)
	    CRASH("Illegal entry type in typesWithUpdate, entry: %ld of type %d, frame: %ld\n", start, type, database->getCurrentFrame());
	  //FIXME this is still happening sometimes, start: 0, type: 0, frame: 11, 273
	  lastType = type;
	}
	handles->update->call(target);
	start++;
      }
      break;
    case WORK_FRAME_END:
      Queue::submitAllForThisThread();
      break;
    }
    sync->start.store(WORK_NONE, std::memory_order_seq_cst);
  }
}

static typename decltype(workerData)::Tentry threadlist[MAX_THREADS];

inline size_t dispatchWork() {//returns number of work units remaining
  size_t i, len, l, remainingUnits;
  do {
    len = workerData.listAll(threadlist, MAX_THREADS);
  } while (!threadlist);
  for(i = 0;i < len;i++) {
    if(threadlist[i]->start.load(std::memory_order_seq_cst) == WORK_NONE) {
      remainingUnits = entitiesWithUpdate.size() - workunitIdx;
      l = glm::min<size_t>(remainingUnits, glm::max<size_t>(4, remainingUnits / (workerThreadCount*2)));
      if(l) {
	threadlist[i]->len = l;
	threadlist[i]->start.store(workunitIdx, std::memory_order_seq_cst);
        //LOG("Sent work for entity %d to thread %d on frame %ld\n", workunitIdx, i, database->getCurrentFrame());
	workunitIdx += l;
      }
    }
  }
  return entitiesWithUpdate.size() - workunitIdx;
}

inline void waitForAllWorkToFinish() {
  while(dispatchWork());//dispatch all
  size_t len, i;
  len = workerData.listAll(threadlist, MAX_THREADS);
  for(i = 0;i < len;i++) {
    while(threadlist[i]->start.load(std::memory_order_seq_cst) != WORK_NONE);
    //TODO wait for all old calls to finish?
    threadlist[i]->start.store(WORK_FRAME_END, std::memory_order_seq_cst);
  }
  for(i = 0;i < len;i++)
    while(threadlist[i]->start.load(std::memory_order_seq_cst) != WORK_NONE);
}

extern void updateTime();
extern void initTime();

void enterMainLoop() {
  masterState = 1;
  size_t i;
  static std::atomic<int8_t> meshSemaphore;//static so it's not on a thread stack
  int8_t tempu8;
  initTime();
  meshSemaphore.store(0, std::memory_order_relaxed);
  WITE::Thread::spawnThread(WITE::Thread::threadEntry_t_F::make<void*>(&meshSemaphore, &Mesh::proceduralMeshLoop));
  auto workerEntry = WITE::Thread::threadEntry_t_F::make<void*>(NULL, &enterWorker);
  for(i = 0;i < workerThreadCount;i++) WITE::Thread::spawnThread(workerEntry);
  do {//wait for first procedural mesh loop to complete
    //TODO I really shouldn't need to count on this. Render code should check if the mesh is real before rendering.
    tempu8 = meshSemaphore.load(std::memory_order_consume);
  } while(!tempu8);
  vertBuffer = tempu8 - 1;
  meshSemaphore.store(0, std::memory_order_release);
  while(true) {
    TIME(database->advanceFrame(), 1, "advance frame (db wait): %llu\n");//no updates on frame 0.
    if(masterState.load(std::memory_order_relaxed) == 2) break;
    updateTime();
    entitiesWithUpdate.clear();
    workunitIdx = 0;
    TIME(database->getEntriesWithUpdate(&entitiesWithUpdate), 2, "entry setup: %llu\n");
    TIME(dispatchWork(), 2, "first dispatch: %llu\n");//TODO specify longer total jobs
    TIME(while(!Window::areRendersDone()) dispatchWork(), 1, "Wait for render: %llu\n");//TODO work units performed here give a metric for comparative load
    TIME(Window::pollAllEvents(), 2, "Event polling: %llu\n");
    TIME(waitForAllWorkToFinish(), 1, "Wait for workers: %llu\n");
    TIME(Window::presentAll(), 1, "Present: %llu\n");//this may wait for vblank, if the fifo queue is full
    tempu8 = meshSemaphore.load(std::memory_order_acquire);
    if(tempu8) {//vertex buffer is tripple-buffered so allowing writes to the next one won't bother reads in progress
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
