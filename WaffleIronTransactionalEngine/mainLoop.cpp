#includ "mainLoop.h"

void enterWorker(void* unused) {
  Database::Entry target;
  size_t start, end;
  workerData_t* sync = workerData.get();
  Database::typeHandles* handles = NULL;
  Database::type lastType = 0, type;
  while(true) {
    do {
      start = sync->start.load(std::memory_order_aquire);
    } while(start == WORK_NONE);
    len = sync->len + start;
    while(start < len) {
      target = entitiesWithUpdate[start];
      type = Database::getEntryType(target);
      if(!handles || type != lastType) {
	handles = Database::getHandlesOfType(type);
	lastType = type;
      }
      handles->update.call(target, NULL);//TODO return value is uint64_t, what is it for?
      start++;
    }
    start.store(WORK_NONE, std::memory_order_release);
  }
}

inline size_t dispatchWork() {//returns number of work units dispatched
  size_t i, len, ret = 0, l;
  workerData::Tentry* threadlist;
  len = workerData.listAll(&threadlist);
  for(i = 0;i < len;i++) {
    if(threadlist[i].exists && threadlist[i].obj.start.load(std::memory_order_aquire) == WORK_NONE) {
      l = glm::min(entitiesWithUpdate.size() - workunitIdx, glm::max(4, (entitiesWithUpdate.size() - workunitIdx) / (workerThreadCount*2)));
      if(l) {
	workunitIdx += l;
	ret += l;
	threadlist[i].obj.len = l;
	threadlist[i].obj.start.store(workunitIdx, std::memory_order_release);
      }
    }
  }
  return ret;
}

export void enterMainLoop(Database* db) {
  uint64_t renderFrame, updateFrame;
  size_t i;
  static std::atomic_uint8_t meshSemaphore;
  uint8_t tempu8;
  database = db;//global assign
  meshSemaphore.store(0, std::memory_order_relaxed);
  spawnThread(Thread::threadEntry_t::make(NULL, &meshSemaphore, &VMesh::proceduralMeshLoop));
  for(i = 0;i < workerThreadCount;i++) spawnThread(Thread::threadEntry_t::make(NULL, &enterWorker));
  do {
    tempu8 = meshSemaphore.load(std::memory_order_consume);
    dispatchWork();
  } while(!tempu8);
  vertBuffer = tempu8 - 1;
  meshSemaphore.store(0, std::memory_order_release);
  while(true) {
    dispatchWork();
    WITE::Window::renderAll();// note: includes load of transforms of workers at renderFrame
    while(!dispatchWork());
    database->advanceFrame();
    entitiesWithUpdate.clear();
    database->getEntriesWithUpdate(&entitiesWithUpdate);
    workunitIdx = 0;
    while(!WITE::Window::areRendersDone()) dispatchWork();//TODO work units performed here give a metric for comparative load
    //wait for vsync/frame-cap, if any
    tempu8 = meshSemaphore.load(std::memory_order_consume);
    if(tempu8) {
      vertBuffer = tempu8 - 1;
      meshSemaphore.store(0, std::memory_order_release);
    }
  }
}
