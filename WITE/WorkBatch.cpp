#include "WorkBatch.hpp"
#include "Queue.hpp"

namespace WITE::GPU {

  bool WorkBatch::WorkBatchResources::canRecord() {
    return state == state_t::recording;
  };

  Work& WorkBatch::WorkBatchResources::getCurrent() {
    return steps.empty() ? steps.emplace_back() : steps[steps.size()-1];
  };

  Work& WorkBatch::WorkBatchResources::append() {
    steps.emplace_back();
  };

  void WorkBatch::WorkBatchResources::reset() {//only reset when consigning to the recycler
    VK_ASSERT(state == state_t::done, "attempted to reset when not done");
    state = state_t::recording;
    cycle++;
    waiters.clear();
    prerequisits.clear();
    steps.clear();
  };

  bool WorkBatch::WorkBatchResources::canStart() {
    if(state != state_t::pending)
      return false;
    for(auto& pre : prerequisits)
      if(!pre.isDone())
	return false;
    return true;
  };

  BalancingThreadResourcePool<WorkBatch::WorkBatchResources> allBatches;//static

  WorkBatch::WorkBatchResources* WorkBatch::get() {
    return batch && cycle == batch->cycle ? batch : NULL;
  };

  WorkBatch::WorkBatch(uint64_t gpuIdx) {
    batch = allBatches.allocate();
    cycle = batch.cycle;
  };

  void WorkBatch::init() {//static
    TODO;//spawn state polling threads
  };

  void WorkBatch::check() {
    auto b = get();
    if(!b) return;
    Util::ScopeLock lock(&b->mutex);
    if(b.cycle != cycle) return;
    switch(b->state) {
    case recording: return;
    case pending:
      if(!b->canStart()) return;
      b->state = state_t::running;
      if(!b->steps.empty()) {
	b->currentStep = 0;
	execute(b->steps[0]);
      }
    case running:
      b->?TODO?;
    };
    //TODO;//
  };

  void WorkBatch::execute(Work& w) {
    if(w->cpu) {
      w->cpu(*this);
      w->cpu = NULL;
    }
    w->waitedSemsStaging.clear();
    w->cmdStaging.clear();
    w->submitStaging.clear();
    //TODO
  };

  #define PreRecordBoilerplate \
    auto b = get(); \
    ASSERT_TRAP(b, "attempted to append to very stale batch"); \
    Util::ScopeLock lock(&b->mutex); \
    ASSERT_TRAP(b->canRecord() && b->cycle == cycle, "invalid state"); \
    auto& current = b->getCurrent(); \
    //end pre-record boilerplate. This is a macro bc must emit the lock to the calling scope.

  WorkBatch WorkBatch::then(WorkBatch::thenCB cb) {
    PreRecordBoilerplate;
    if(current.cpu || current.q)
      current = b.append();
    current.cpu = cb;
    return *this;
  };

  WorkBatch WorkBatch::thenOnGpu(size_t gpuIdx, thenCB cb) {
    PreRecordBoilerplate;
    if(current.gpu != gpuIdx && current.gpu != ~0)
      current = b.append();
    current.gpu = gpuIdx;
    return *this;
  };

  WorkBatch WorkBatch::thenOnAGpu(logicalDeviceMask_t ldm) {
    PreRecordBoilerplate;
    if(!Gpu::ldmContains(ldm, current->gpu))
      thenOnGpu(Gpu::getGpuFor(ldm));
    return *this;
  };

  const WorkBatch WorkBatch::submit() {
    PreRecordBoilerplate;//submit requires it be in the recording state too
    state = state_t::pending;
    //potential future enhancement: check if all prerequisits are also on this gpu and if so submit this now linked by semaphore
    lock.release();
    check();//see if this can begin and if so, do it
    return *this;
  };

  void WorkBatch::mustHappenAfter(WorkBatch& o) {
    PreRecordBoilerplate;
    b->prerequisits.push_back(*this);
    auto ob = o.get();
    if(ob && !ob.isDone()) {
      Util::ScopeLock olock(&ob->mutex);
      ob->waiters.push_back(*this);
    }
  };

  bool WorkBatch::isDone() {
    auto b = get();
    if(!b)
      return true;
    Util::ScopeLock lock(&b->mutex);
    return b->state == state_t::done || b->cycle != cycle;
    //cycle check covers race condition if b recycled before the lock was taken
  };

  bool WorkBatch::isSubmitted() {
    auto b = get();
    if(!b)
      return true;
    Util::ScopeLock lock(&b->mutex);
    return b->state != state_t::running || b->cycle != cycle;
  };

  bool WorkBatch::isWaiting() {
    auto b = get();
    if(!b)
      return true;
    Util::ScopeLock lock(&b->mutex);
    return b->state == state_t::pending || b->cycle != cycle;
  };

  //TODO more
};
