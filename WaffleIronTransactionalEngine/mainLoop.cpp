#include "constants.h"
#include "mainLoop.h"
#include "Mesh.h"
#include "Window.h"

void enterWorker(void* unused) {
  WITE::Database::Entry target;
  size_t start, end;
  std::shared_ptr<workerData_t> sync = workerData.get();
  WITE::Database::typeHandles* handles = NULL;
  WITE::Database::type lastType = 0, type;
  while(true) {
    do {
      start = sync->start.load(std::memory_order_acquire);
    } while(start == WORK_NONE);
    end = sync->len + start;
    while(start < end) {
      target = entitiesWithUpdate[start];
      type = database->getEntryType(target);
      if(!handles || type != lastType) {
	handles = WITE::Database::getHandlesOfType(type);
	lastType = type;
      }
      handles->update->call(target);//TODO return value is uint64_t, what is it for?
      start++;
    }
    sync->start.store(WORK_NONE, std::memory_order_release);
  }
}

inline size_t dispatchWork() {//returns number of work units dispatched
  size_t i, len, ret = 0, l, remainingUnits;
  typename decltype(workerData)::Tentry* threadlist;
  len = workerData.listAll(&threadlist);
  for(i = 0;i < len;i++) {
    if(threadlist[i] && threadlist[i]->start.load(std::memory_order_acquire) == WORK_NONE) {
      remainingUnits = entitiesWithUpdate.size() - workunitIdx;
      l = glm::min<size_t>(remainingUnits, glm::max<size_t>(4, remainingUnits / (workerThreadCount*2)));
      if(l) {
	workunitIdx += l;
	ret += l;
	threadlist[i]->len = l;
	threadlist[i]->start.store(workunitIdx, std::memory_order_release);
      }
    }
  }
  return ret;
}

export_def void enterMainLoop(WITE::Database* db) {
  size_t i;
  static std::atomic_uint8_t meshSemaphore;
  uint8_t tempu8;
  database = db;//global assign
  meshSemaphore.store(0, std::memory_order_relaxed);
  WITE::Thread::spawnThread(WITE::Thread::threadEntry_t_F::make(&meshSemaphore, &Mesh::proceduralMeshLoop));
  auto workerEntry = WITE::Thread::threadEntry_t_F::make(NULL, &enterWorker);
  for(i = 0;i < workerThreadCount;i++) WITE::Thread::spawnThread(workerEntry);
  do {
    tempu8 = meshSemaphore.load(std::memory_order_consume);
    dispatchWork();
  } while(!tempu8);
  vertBuffer = tempu8 - 1;
  meshSemaphore.store(0, std::memory_order_release);
  while(true) {
    dispatchWork();//TODO specify longer total jobs
    Window::renderAll();// note: includes load of transforms of objects at renderFrame
    while(!dispatchWork());
    database->advanceFrame();
    entitiesWithUpdate.clear();
    database->getEntriesWithUpdate(&entitiesWithUpdate);
    workunitIdx = 0;
    while(!Window::areRendersDone()) dispatchWork();//TODO work units performed here give a metric for comparative load
    //wait for vsync/frame-cap, if any
    tempu8 = meshSemaphore.load(std::memory_order_consume);
    if(tempu8) {
      vertBuffer = tempu8 - 1;
      meshSemaphore.store(0, std::memory_order_release);
    }
  }
}
