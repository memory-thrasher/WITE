#include "WorkBatch.hpp"
#include "Queue.hpp"
#include "constants.hpp"
#include "Thread.hpp"
#include "Gpu.hpp"
#include "Image.hpp"

namespace WITE::GPU {

  void WorkBatch::Work::setGpu(size_t gnu) {
    gpu = gnu;
    if(completeSem)
      completeSem->dispose();
    if(~gnu) {
      completeSem = makeSharedSemaphore(gnu);
      ASSERT_TRAP(completeSem->get(), "failed to allocate semaphore");
    }
  };

  bool WorkBatch::WorkBatchResources::canRecord() {
    return state == state_t::recording;
  };

  WorkBatch::Work& WorkBatch::WorkBatchResources::getCurrent() {
    return steps.empty() ? steps.emplace_back() : steps[steps.size()-1];
  };

  WorkBatch::Work& WorkBatch::WorkBatchResources::append() {
    return steps.emplace_back();
  };

  void WorkBatch::WorkBatchResources::reset() {//only reset when consigning to the recycler
    ASSERT_TRAP(state == state_t::done, "attempted to reset when not done");
    for(auto w : steps)
      ASSERT_TRAP(!w.cmd || !w.completeSem->pending(), "attempted to reset but the cmd has been allocated");
    cycle++;
    state = state_t::recording;
    // waiters.clear();
    prerequisits.clear();
    for(Work& w : steps) {
      if(w.fence) {
	fences[w.getGpu()]->free(w.fence);
	w.fence = NULL;
      }
      if(w.cmd) {
	w.q->free(w.cmd);
	new(&w.cmd)cmd_t();
      }
      if(w.completeSem)
	w.completeSem->dispose();
      if(w.q)
	w.q = NULL;
    }
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

  Collections::IterableBalancingThreadResourcePool<WorkBatch::WorkBatchResources> WorkBatch::allBatches;//static
  PerGpuUP<Collections::BalancingThreadResourcePool<vk::Fence>> WorkBatch::fences;
  uint32_t WorkBatch::promiseThreads[PROMISE_THREAD_COUNT];
  std::atomic_bool WorkBatch::promiseThreadsRunning[PROMISE_THREAD_COUNT];

  WorkBatch::WorkBatch(uint64_t gpuIdx) {
    batch = allBatches.allocate();
    cycle = batch->cycle;
    batch->startFrame = Util::FrameCounter::getFrame();
    batch->getCurrent().setGpu(gpuIdx);
  };

  WorkBatch::WorkBatch(logicalDeviceMask_t ldm) : WorkBatch(Gpu::getGpuFor(ldm).getIndex()) {};

  WorkBatch::WorkBatch(Queue* q) : WorkBatch(q->getGpuIdx()) {
    get()->getCurrent().q = q;
  };

  void WorkBatch::init() {//static
    for(uint32_t i = 0;i < PROMISE_THREAD_COUNT;i++) {
      Platform::Thread::spawnThread(Platform::Thread::threadEntry_t_F::make<uint32_t>(i, checkWorkerLoop));
    }
  };

  void WorkBatch::checkWorkerLoop(uint32_t idx) {//static
    promiseThreadsRunning[idx] = true;
    promiseThreads[idx] = Platform::Thread::getCurrentTid();
    while(Gpu::running) {
      WorkBatchResources* wbr = allBatches.getAny();//resilient semi-ordered iteration
      if(!wbr) continue;//list empty: busy wait
      wbr->check();
    }
    promiseThreadsRunning[idx] = false;
  };

  void WorkBatch::joinPromiseThreads() {
    for(size_t i = 0;i < PROMISE_THREAD_COUNT;i++) {
      if(promiseThreads[i] == Platform::Thread::getCurrentTid()) continue;
      while(promiseThreadsRunning[i]) Platform::Thread::sleepShort();
    }
  };

  void WorkBatch::WorkBatchResources::check() {
    Util::AdvancedScopeLock lock(mutex, 0);
    if(!lock.isHeld()) return;//if another thread has this locked, don't check anything but also don't wait
    switch(state) {
    case state_t::recording: return;
    case state_t::pending:
      if(!canStart()) return;//TODO eventually allow b to start when a prerequisite is in progress if a semaphore can be used
      if(!steps.empty()) {
	state = state_t::running;
	step = 0;
	execute();
      } else {
	state = state_t::done;
      }
      break;
    case state_t::running:
      {
	if(step >= steps.size()) {//all submitted
	  auto ss = steps.size();
	  if(ss > 0 && (!steps[ss-1].completeSem || !steps[ss-1].completeSem->pending()))
	    state = state_t::done;
	  else
	    break;
	} else {
	  execute();
	  if(state != state_t::done)
	    break;
	}
      }
    case state_t::done:
      //TODO proc waiters if any. Need deadlock resolution, see comment above mustHappenAfter
      reset();
      allBatches.free(this);
    };
  };


  WorkBatch::WorkBatch(WorkBatchResources* batch) : batch(batch), cycle(batch->cycle) {};

  void WorkBatch::WorkBatchResources::execute() {
    do {
      ASSERT_TRAP(state == state_t::running, "attempted to execute on invalid state");
      ASSERT_TRAP(step < steps.size(), "out of bounds");
      Work* w = &steps[step];
      if(w->cpu) {
	switch(w->cpu(this)) {
	case result::done:
	  w->cpu = NULL;
	  break;
	case result::yield:
	  return;
	};
      }
      if(w->fence) {
	auto res = Gpu::get(w->getGpu()).getVkDevice().getFenceStatus(*w->fence);
	// LOG("Fence: status of ", *w->fence, " is ", res);
	switch(res) {
	case vk::Result::eSuccess:
	  fences[w->getGpu()]->free(w->fence);
	  w->fence = NULL;
	  break;
	case vk::Result::eNotReady:
	  return;
	default:
	  VK_ASSERT(res, "Failure detected while waiting fence");
	}
      }
      if(w->q && w->cmd) {
	ASSERT_TRAP(w->completeSem, "workbatch has cmd but no completion semaphore");
	if(w->completeSem->pending()) return;
	waitedSemsStaging.clear();
	do {
	  w = &steps[step];
	  cmdStaging.clear();
	  signalSemsStaging.clear();
	  for(;step < steps.size();step++) {
	    Work* w2 = &steps[step];
	    if(w2->cpu || w2->fence || w2->q != w->q)
	      break;
	    cmdStaging.emplace_back(*w2->cmd);
	    w2->cmd->end();
	    signalSemsStaging.push_back(w2->completeSem->get()->prepareForBatchSubmit());
	    ASSERT_TRAP(w2->completeSem->pending(), "semaphore is to be signaled but is not pending");
	  }
	  vk::SubmitInfo2 si { {},
	    static_cast<uint32_t>(waitedSemsStaging.size()), waitedSemsStaging.data(),
	    static_cast<uint32_t>(cmdStaging.size()), cmdStaging.data(),
	    static_cast<uint32_t>(signalSemsStaging.size()), signalSemsStaging.data()
	  };
	  ASSERT_TRAP(signalSemsStaging.size(), "Submitted a cmd with no signal semaphore ", w->cmd);
	  w->q->submit(si);
	  //for next loop:
	  waitedSemsStaging.clear();
	  waitedSemsStaging.push_back(signalSemsStaging[signalSemsStaging.size()-1]);
	} while(step < steps.size() && steps[step].getGpu() == w->getGpu() && !steps[step].cpu && !steps[step].fence);
	return;
      }
      step++;
    } while(step < steps.size());
  };

  vk::CommandBuffer WorkBatch::WorkBatchResources::getCmd(QueueType qt) {
    Work* c = &getCurrent();
    ASSERT_TRAP(~c->getGpu(), "get cmd called when no gpu has been chosen (need a thenOnGpu first)");
    if(c->q && c->q->supports(qt)) {
      [[likely]]
      if(!c->cmd)
	c->cmd = c->q->getNext();
      return *c->cmd;
    }
    size_t gpu = c->getGpu();
    if(c->q) {
      c = &append();
      c->setGpu(gpu);
    }
    c->q = Gpu::get(gpu).getQueue(qt);
    return *(c->cmd = c->q->getNext());
  };

  vk::CommandBuffer WorkBatch::WorkBatchResources::getCmd() {//used for commands that do not care which queue is used
    Work* c = &getCurrent();
    ASSERT_TRAP(~c->getGpu(), "get cmd called when no gpu has been chosen (need a thenOnGpu first)");
    if(c->q) {
      [[likely]]
      if(!c->cmd) [[unlikely]]
	c->cmd = c->q->getNext();
    } else {
      c->q = Gpu::get(c->getGpu()).getQueue(QueueType::eTransfer);
      c->cmd = c->q->getNext();
    }
    return *c->cmd;
  };

  uint32_t WorkBatch::currentQFam() {
    getCmd();//ensures a q has been picked
    return get()->getCurrent().q->family;
  };

  #define PreRecordBoilerplate \
    auto b = get(); \
    ASSERT_TRAP(b, "attempted to append to very stale batch"); \
    Util::AdvancedScopeLock lock(b->mutex); \
    ASSERT_TRAP(b->canRecord() && b->cycle == cycle, "invalid state"); \
    [[maybe_unused]] auto current = &b->getCurrent(); \
    //end pre-record boilerplate. This is a macro bc must emit the lock to the calling scope.

  void WorkBatch::reset() {
    PreRecordBoilerplate;
    b->state = state_t::done;
    b->reset();
  };

  WorkBatch WorkBatch::then(WorkBatch::thenCB cb) {
    PreRecordBoilerplate;
    if(current->cpu || current->q)
      current = &b->append();
    current->cpu = cb;
    return *this;
  };

  WorkBatch WorkBatch::thenOnGpu(size_t gpuIdx, thenCB cb) {
    PreRecordBoilerplate;
    if(current->getGpu() != gpuIdx && current->getGpu() != ~0)
      current = &b->append();
    current->setGpu(gpuIdx);
    return *this;
  };

  WorkBatch WorkBatch::thenOnAGpu(logicalDeviceMask_t ldm) {
    PreRecordBoilerplate;
    if(!Gpu::ldmContains(ldm, current->getGpu()))
      thenOnGpu(Gpu::getGpuFor(ldm).getIndex());
    return *this;
  };

  WorkBatch WorkBatch::submit() {
    PreRecordBoilerplate;//submit requires it be in the recording state too
    ASSERT_TRAP(!b->isRecordingRP, "attempted to submit a command buffer while a render pass is being recorded (missing endRenderPass)");
    b->state = state_t::pending;
    //potential future enhancement: check if all prerequisits are also on this gpu and if so submit this now linked by semaphore
    lock.release();
    b->check();//see if this can begin and if so, do it
    return *this;
  };

  /*
    waiter signalling race condition:
    thread 1: X.mustHappenAfter(Y)
    thread 2: Y.check() when done

    Thread 1: acquires X.mutex, passed if bc Y.mutex not yet held, execution yields immediately after if
    Thread 2: acquires Y.mutex, execution yields at start of switch, Y already in done state
    Thread 1: attempts to acquire Y.mutex to push to Y.waiters, but already held by Thread 2
    Thread 2: attempts to acquire X.mutex to iterate waiters, but already held by Thread 1
    deadlock
    For now, just don't use waiters
   */
  void WorkBatch::mustHappenAfter(WorkBatch& o) {
    PreRecordBoilerplate;
    b->prerequisits.push_back(o);
    // auto ob = o.get();
    // if(ob && !ob.isDone() && !ob->mutex.isHeld()) {
    //   //inclusion in waiters is optional. If missing, check will proc it. Better than holding up this thread.
    //   Util::ScopeLock olock(&ob->mutex);
    //   ob->waiters.push_back(*this);
    // }
  };

  bool WorkBatch::isDone() const {
    auto b = get();
    if(!b)
      return true;
    Util::AdvancedScopeLock lock(b->mutex, 0);
    if(!lock.isHeld())
      return false;
    return b->state == state_t::done || b->cycle != cycle;
    //cycle check covers race condition if b recycled just now
  };

  bool WorkBatch::isSubmitted() const {
    auto b = get();
    if(!b)
      return true;
    //NOTE dirty read, may fail sporadically
    return b->state != state_t::recording || b->cycle != cycle;
  };

  WorkBatch& WorkBatch::putImageInLayout(ImageBase* img, vk::ImageLayout l, QueueType qt,
				   vk::AccessFlags2 a, vk::PipelineStageFlags2 s) {
    putImageInLayout(img, l, getGpu().getQueue(qt)->family, a, s);
    return *this;
  };

  WorkBatch& WorkBatch::putImageInLayout(ImageBase* img, vk::ImageLayout l, uint32_t qFam,
				   vk::AccessFlags2 a, vk::PipelineStageFlags2 s) {
    img->accessStateTracker.getRef(getGpuIdx()).inState({ l, a, s, qFam }, *this);
    return *this;
  };

  // bool WorkBatch::isWaiting() {
  //   auto b = get();
  //   if(!b)
  //     return true;
  //   Util::ScopeLock lock(&b->mutex);
  //   return b->state == state_t::pending || b->cycle != cycle;
  // };

  size_t WorkBatch::getGpuIdx() {
    auto b = get();
    Util::AdvancedScopeLock lock(b->mutex);
    size_t ret = b->getCurrent().getGpu();
    ASSERT_TRAP(~ret, "requested gpu when one hasn't been chosen (need a thenOnGpu first)");
    return ret;
  };

  Gpu& WorkBatch::getGpu() {
    return Gpu::get(getGpuIdx());
  };

  //begin duplication of vk::cmd functions

  WorkBatch& WorkBatch::pipelineBarrier2(vk::DependencyInfo* di) {
    PreRecordBoilerplate;
    getCmd().pipelineBarrier2(di);
    return *this;
  };

  //TODO put the batching barriers back
  void WorkBatch::batchDistributeAcrossLdm(size_t srcGpu, size_t cntAll, ImageBase** srcs, BufferBase** stagings, void** ram) {//static
    constexpr static size_t maxPerBatch = 64;
    vk::ImageMemoryBarrier2 mbs[maxPerBatch * 2];
    vk::DependencyInfo di;
    di.pImageMemoryBarriers = mbs;
    //NOTE: neither image copy nor memory barrier need any specific queue type, so use whatever owns the first image
    // uint32_t q = srcs[0]->accessStateTracker.getRef(srcGpu).get().queueFam;

#define appendTransition(gpu, img, _layout, access, stage, cmd) {	\
      ImageBase::accessState gnu = { _layout,				\
	vk::AccessFlagBits2::e ##access,				\
	vk::PipelineStageFlagBits2::e ##stage, q			\
      }, old = img->accessStateTracker.getRef(gpu).onExternalChange(gnu, cmd); \
      if(old.layout != gnu.layout || old.queueFam != gnu.queueFam)	\
	mbs[mbCnt++] = { old.stageMask, old.accessMask, gnu.stageMask, gnu.accessMask, \
	  gnu.layout, gnu.layout, old.queueFam, gnu.queueFam,		\
	  img->getVkImage(gpu), img->getAllInclusiveSubresource() };	\
    }
#define appendDiscard(gpu, img, _layout, access, stage, cmd) {		\
      ImageBase::accessState gnu = { vk::ImageLayout::e ##_layout,	\
	vk::AccessFlagBits2::e ##access,				\
	vk::PipelineStageFlagBits2::e ##stage, q			\
      }, old = img->accessStateTracker.getRef(gpu).onExternalChange(gnu, cmd); \
      mbs[mbCnt++] = { old.stageMask, old.accessMask, gnu.stageMask, gnu.accessMask, \
	vk::ImageLayout::eUndefined, gnu.layout, old.queueFam, gnu.queueFam, \
	img->getVkImage(gpu), img->getAllInclusiveSubresource() };	\
    }
#define doBarrier(cmd) if(mbCnt) {		\
      di.imageMemoryBarrierCount = mbCnt;	\
      cmd.pipelineBarrier2(&di);		\
      mbCnt = 0;				\
    }

    while(cntAll) {
      size_t cnt = std::min(cntAll, maxPerBatch);
      // size_t mbCnt = 0;
      WorkBatch srcCmd(srcGpu);
      // WorkBatch cleanupCmd;
      //host access requires linear tiling and general layout. General layout allows all operation.
      // for(size_t i = 0;i < cnt;i++) {
      // 	ASSERT_TRAP(((stagings && stagings[i] ? stagings[i] : srcs[i])->slotData.usage & GpuResource::MUSAGE_ANY_HOST) ==
      // 		    GpuResource::MUSAGE_ANY_HOST,
      // 		    "Attempted to batch distribute an image that is not host read/writable (copy requires linear tiling)");
      // 	if(stagings && stagings[i]) {
      // 	  appendDiscard(srcGpu, stagings[i], General, TransferWrite | vk::AccessFlagBits2::eHostRead,
      // 			Copy | vk::PipelineStageFlagBits2::eHost, srcCmd);
      // 	  appendTransition(srcGpu, srcs[i], vk::ImageLayout::eTransferSrcOptimal, TransferRead, Copy, srcCmd);
      // 	} else {
      // 	  appendTransition(srcGpu, srcs[i], vk::ImageLayout::eGeneral, HostRead, Host, srcCmd);
      // 	}
      // }
      // doBarrier(srcCmd);
      //copying parameters not currently required because this is currently only called from BackedRenderTarget which provides permanent backings. Saving this code for future use when that's no longer the case.
      // void** ramCopy = new void*[cnt];
      // memcpy(ramCopy, ram, cnt * sizeof(void*));
      // ImageBase** srcCopy = new ImageBase*[cnt];
      // for(size_t i = 0;i < cnt;i++) {
      // 	if(stagings && stagings[i]) {
      // 	  srcCopy[i] = stagings[i];
      // 	  srcCmd.copyImage(srcs[i], stagings[i]);
      // 	} else {
      // 	  srcCopy[i] = srcs[i];
      // 	}
      // }
      // srcCmd.then([srcGpu, cnt, ramCopy, srcCopy]() {
      srcCmd.thenOnGpu(srcGpu);
      if(stagings)
	for(size_t i = 0;i < cnt;i++)
	  if(stagings[i])
	    srcCmd.copyImage(srcs[i], stagings[i]);
      srcCmd.then([srcGpu, cnt, ram, srcs, stagings]() {
	for(size_t i = 0;i < cnt;i++)
	  if(stagings && stagings[i])
	    stagings[i]->read(ram[i], srcs[i]->getMemSize(srcGpu), srcGpu);
	  else
	    srcs[i]->read(ram[i], srcs[i]->getMemSize(srcGpu), srcGpu);
      });
      srcCmd.submit();
      // cleanupCmd.mustHappenAfter(srcCmd);
      // cleanupCmd.then([ramCopy, srcCopy](){
      // 	delete [] ramCopy;
      // 	delete [] srcCopy;
      // });
      //note: splitting to one cmd per gpu so they can be parallel
      for(size_t gpu = 0;gpu < Gpu::getGpuCount();gpu++) {
	if(gpu == srcGpu || !Gpu::ldmContains(srcs[srcGpu]->slotData.logicalDeviceMask, gpu)) continue;
	WorkBatch cmd(gpu);
	cmd.mustHappenAfter(srcCmd);
	// for(size_t i = 0;i < cnt;i++) {
	//   if(stagings && stagings[i]) {
	//     ASSERT_TRAP(stagings[i]->getMemSize(srcGpu) == stagings[i]->getMemSize(gpu),
	// 		"Ram size of same image format/dimensions is different on different gpus when attempting to copy");
	//     appendDiscard(gpu, stagings[i], General, TransferRead | vk::AccessFlagBits2::eHostWrite,
	// 		  Copy | vk::PipelineStageFlagBits2::eHost, cmd);
	//     appendDiscard(gpu, srcs[i], TransferDstOptimal, TransferRead, Copy, cmd);
	//   } else {
	//     ASSERT_TRAP(srcs[i]->getMemSize(srcGpu) == srcs[i]->getMemSize(gpu),
	// 		"Ram size of same image format/dimensions is different on different gpus when attempting to copy");
	//     appendDiscard(gpu, srcs[i], Preinitialized, HostWrite, Host, cmd);
	//   }
	// }
	// doBarrier(cmd);
	cmd.then([gpu, cnt, srcs, stagings, ram, srcGpu]() {
	  for(size_t i = 0;i < cnt;i++)
	    if(stagings && stagings[i])
	      stagings[i]->write(ram[i], srcs[i]->getMemSize(srcGpu), gpu);
	    else
	      srcs[i]->write(ram[i], srcs[i]->getMemSize(srcGpu), gpu);
	});
	if(stagings)
	  for(size_t i = 0;i < cnt;i++)
	    if(stagings[i])
	      cmd.copyImage(stagings[i], srcs[i]);
	cmd.submit();
	// cleanupCmd.mustHappenAfter(cmd);
      }
      // cleanupCmd.submit();
      cntAll -= cnt;
      srcs += cnt;
      if(stagings) stagings += cnt;
    }
  };
#undef appendTransition
#undef appendDiscard

  WorkBatch& WorkBatch::readImage(ImageBase* img, void* ram) {
    //binary data is only meaningful to non-device observers in general layout
    putImageInLayout(img, vk::ImageLayout::eGeneral, QueueType::eTransfer,
		     vk::AccessFlagBits2::eHostRead, vk::PipelineStageFlagBits2::eHost);
    auto gpu = getGpuIdx();
    then([img, ram, gpu](){ img->mem.getPtr(gpu)->read(ram); });
    return *this;
  };

  WorkBatch& WorkBatch::writeImage(ImageBase* img, void* ram) {
    putImageInLayout(img, vk::ImageLayout::eGeneral, QueueType::eTransfer,
		     vk::AccessFlagBits2::eHostWrite, vk::PipelineStageFlagBits2::eHost);
    auto gpu = getGpuIdx();
    then([img, ram, gpu](){ img->mem.getPtr(gpu)->write(ram); });
    return *this;
  };

  WorkBatch& WorkBatch::readBuffer(BufferBase* b, void* ram) {
    auto gpu = getGpuIdx();
    then([b, ram, gpu](){ b->mem.getPtr(gpu)->read(ram); });
    return *this;
  };

  WorkBatch& WorkBatch::writeBuffer(BufferBase* b, void* ram) {
    auto gpu = getGpuIdx();
    then([b, ram, gpu](){ b->mem.getPtr(gpu)->write(ram); });
    return *this;
  };

  WorkBatch& WorkBatch::beginRenderPass(const renderInfo* ri) {
    PreRecordBoilerplate;
    ASSERT_TRAP(!b->isRecordingRP, "Duplicate RP begin");
    b->isRecordingRP = true;
    b->currentRP = *ri;
    auto cmd = getCmd(QueueType::eGraphics);
    size_t gpuIdx = getGpuIdx();
    uint32_t q = current->q->family;
    for(size_t i = 0;i < b->currentRP.resourceCount;i++) {
      b->currentRP.resources[i]->accessStateTracker.getRef(gpuIdx).inState({
	  b->currentRP.initialLayouts[i],
	  vk::AccessFlagBits2::eShaderWrite,
	  vk::PipelineStageFlagBits2::eAllGraphics, q
	}, *this);
    }
    // LOG("Begin render pass with fb ", b->currentRP.rpbi.framebuffer, " on frame ", Util::FrameCounter::getFrame());
    cmd.beginRenderPass(&b->currentRP.rpbi, b->currentRP.sc);
    return *this;
  };

  WorkBatch& WorkBatch::endRenderPass() {
    PreRecordBoilerplate;
    ASSERT_TRAP(b->isRecordingRP, "RP end when not in an RP");
    b->isRecordingRP = false;
    // size_t gpuIdx = getGpuIdx();
    // for(size_t i = 0;i < b->currentRP.resourceCount;i++)
    //   b->currentRP.resources[i]->accessStateTracker.getRef(gpuIdx).inState({ b->currentRP.finalLayouts[i],
    // 	  vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
    // 	  vk::PipelineStageFlagBits2::eBottomOfPipe,
    // 	  b->getCurrent().q->family
    // 	}, *this);
    getCmd().endRenderPass();
    return *this;
  };

  WorkBatch& WorkBatch::copyImage(ImageBase* src, ImageBase* dst) {
    PreRecordBoilerplate;
    auto cmd = getCmd();
    size_t gpuIdx = getGpuIdx();
    auto srcLayout = src->accessStateTracker.getRef(gpuIdx).get().layout;
    if(srcLayout != vk::ImageLayout::eGeneral) srcLayout = vk::ImageLayout::eTransferSrcOptimal;
    putImageInLayout(src, srcLayout, currentQFam(), vk::AccessFlagBits2::eTransferRead, vk::PipelineStageFlagBits2::eCopy);
    auto dstLayout = dst->accessStateTracker.getRef(gpuIdx).get().layout;
    if(dstLayout != vk::ImageLayout::eGeneral) dstLayout = vk::ImageLayout::eTransferDstOptimal;
    putImageInLayout(dst, dstLayout, currentQFam(), vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eCopy);
    ASSERT_TRAP(src->getVkSize3D() == dst->getVkSize3D(), "copy size mismatch");
    vk::ImageCopy r(src->getAllSubresourceLayers(), {}, dst->getAllSubresourceLayers(), {}, src->getVkSize3D());
    cmd.copyImage(src->getVkImage(gpuIdx), srcLayout, dst->getVkImage(gpuIdx), dstLayout, 1, &r);
    return *this;
  };

  WorkBatch& WorkBatch::copyImage(BufferBase* src, ImageBase* dst) {
    PreRecordBoilerplate;
    auto cmd = getCmd();
    size_t gpuIdx = getGpuIdx();
    putImageInLayout(dst, vk::ImageLayout::eGeneral, currentQFam(), vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eCopy);
    ASSERT_TRAP(dst->getMemSize(gpuIdx) <= src->size, "copy size mismatch");
    vk::BufferImageCopy r;
    r.setImageSubresource(dst->getAllSubresourceLayers()).setImageExtent(dst->getVkSize3D());
    cmd.copyBufferToImage(src->getVkBuffer(gpuIdx), dst->getVkImage(gpuIdx), vk::ImageLayout::eGeneral, 1, &r);
    return *this;
  };

  WorkBatch& WorkBatch::copyImage(ImageBase* src, BufferBase* dst) {
    PreRecordBoilerplate;
    auto cmd = getCmd();
    size_t gpuIdx = getGpuIdx();
    putImageInLayout(src, vk::ImageLayout::eGeneral, currentQFam(), vk::AccessFlagBits2::eTransferRead, vk::PipelineStageFlagBits2::eCopy);
    ASSERT_TRAP(src->getMemSize(gpuIdx) <= dst->size, "copy size mismatch");
    vk::BufferImageCopy r;
    r.setImageSubresource(src->getAllSubresourceLayers()).setImageExtent(src->getVkSize3D());
    cmd.copyImageToBuffer(src->getVkImage(gpuIdx), vk::ImageLayout::eGeneral, dst->getVkBuffer(gpuIdx), 1, &r);
    return *this;
  };

  WorkBatch& WorkBatch::copyImageToVk(ImageBase* src, vk::Image dst, vk::ImageLayout postLayout) {
    PreRecordBoilerplate;
    auto cmd = getCmd();
    size_t gpuIdx = getGpuIdx();
    auto srcLayout = src->accessStateTracker.getRef(gpuIdx).get().layout;
    if(srcLayout != vk::ImageLayout::eGeneral) srcLayout = vk::ImageLayout::eTransferSrcOptimal;
    putImageInLayout(src, srcLayout, currentQFam(), vk::AccessFlagBits2::eTransferRead, vk::PipelineStageFlagBits2::eCopy);
    //just assume they're the same size
    auto tempLayout = postLayout == vk::ImageLayout::eGeneral ? postLayout : vk::ImageLayout::eTransferDstOptimal;
    discardImage(cmd, dst, tempLayout);
    vk::ImageCopy r(src->getAllSubresourceLayers(), {}, src->getAllSubresourceLayers(), {}, src->getVkSize3D());
    cmd.copyImage(src->getVkImage(gpuIdx), srcLayout, dst, tempLayout, 1, &r);
    quickImageBarrier(cmd, dst, tempLayout, postLayout);
    return *this;
  };

  void WorkBatch::quickImageBarrier(vk::CommandBuffer cmd, vk::Image img, vk::ImageLayout pre, vk::ImageLayout post) {//static
    vk::ImageMemoryBarrier2 mb {
      vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone,
      vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone,
      pre, post, 0, 0, img, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    };
    vk::DependencyInfo di;
    di.pImageMemoryBarriers = &mb;
    di.imageMemoryBarrierCount = 1;
    cmd.pipelineBarrier2(&di);
  };

  void WorkBatch::discardImage(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout post) { //static
    quickImageBarrier(cmd, image, vk::ImageLayout::eUndefined, post);
  };

  vk::Fence WorkBatch::WorkBatchResources::useFence() {
    Work* current = &getCurrent();
    ASSERT_TRAP(~current->getGpu(), "Attempted to use fence without supplying a gpu");
    size_t gpuIdx = current->getGpu();
    if(current->fence) {
      [[unlikely]]
      current = &append();
      current->setGpu(gpuIdx);
    }
    vk::Fence* fence = fences.getRef(gpuIdx)->allocate();
    if(!*fence) [[unlikely]] {
      vk::FenceCreateInfo ci;//all default (only option is for fence to be pre-signaled)
      VK_ASSERT(Gpu::get(gpuIdx).getVkDevice().createFence(&ci, NULL, fence), "Failed to create fence");
    } else
      VK_ASSERT(Gpu::get(gpuIdx).getVkDevice().resetFences(1, fence), "Failed to reset fence");
    current->fence = fence;
    return *fence;
  };

  WorkBatch::result WorkBatch::presentImpl2(vk::SwapchainKHR swap, uint32_t target, Queue* q, WorkBatch wb) { //static
    //WorkBatch unused but present to allow this to be a raw thenCB
    vk::PresentInfoKHR pi;
    // LOG("Presenting ", target);
    pi.setSwapchainCount(1)
      .setPSwapchains(&swap)
      .setPImageIndices(&target);
    q->present(&pi);
    return result::done;
  };

  WorkBatch::result WorkBatch::presentImpl(ImageBase* src, vk::Image* swapImages, vk::SwapchainKHR swap,
					   Queue* q, WorkBatch wb) { // static
    uint32_t target;
    WorkBatch stage2(q);
    stage2.setFrame(wb.getFrame());
    stage2.mustHappenAfter(wb);
    auto f = stage2.useFence();
    vk::Result res = q->getGpu()->getVkDevice().acquireNextImageKHR(swap, 0, std::nullptr_t(), f, &target);
    switch(res) {
    case vk::Result::eNotReady:
      // LOG("Resetting fence ", f, " and cancelling work ", stage2.get());
      stage2.reset();
      return result::yield;
    case vk::Result::eSuboptimalKHR: WARN("NYI suboptimal flag received while presenting: should recreate swapchain"); break;
    default:
      VK_ASSERT(res, "Failed to display image");
      break;
    }
    // LOG("acquired image ", target, " with pending signaling of fence ", f, " on Work ", stage2.get());
    //this is in a then. The primary call to present has already queued fence as a requirement for this batch. The copy commands, however, could not be recorded before the output was chosen by acquire above. Batches cannot be added to while running, So spawn a new batch that depends on this one.
    stage2.copyImageToVk(src, swapImages[target], vk::ImageLayout::ePresentSrcKHR);
    stage2.then(thenCB_F::make<vk::SwapchainKHR, uint32_t, Queue*>(swap, target, q, &WorkBatch::presentImpl2));
    stage2.submit();
    // LOG("Work list now has ", allBatches.allocatedCount(), " works scheduled");
    return result::done;
  };

  WorkBatch& WorkBatch::present(ImageBase* src, vk::Image* swapImages, vk::SwapchainKHR swap) {
    PreRecordBoilerplate;
    then(thenCB_F::make<ImageBase*, vk::Image*, vk::SwapchainKHR, Queue*>
	 (src, swapImages, swap, current->q, &presentImpl));
    //acquiring the swapchain image has to signal a semaphore or fence. In a then callback because it can block/timeout.
    return *this;
  };

  vk::Fence WorkBatch::useFence() {
    PreRecordBoilerplate;
    return b->useFence();
  };

  WorkBatch& WorkBatch::bindDescriptorSets(vk::PipelineBindPoint pipelineBindPoint, vk::PipelineLayout layout,
					   uint32_t firstSet, uint32_t descriptorSetCount,
					   const vk::DescriptorSet* pDescriptorSets,
					   uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    PreRecordBoilerplate;
    getCmd().bindDescriptorSets(pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets,
			       dynamicOffsetCount, pDynamicOffsets);
    return *this;
  };

  WorkBatch& WorkBatch::nextSubpass(vk::SubpassContents contentType) {
    PreRecordBoilerplate;
    getCmd().nextSubpass(contentType);
    return *this;
  };

  WorkBatch& WorkBatch::bindPipeline(vk::PipelineBindPoint pipelineBindPoint, vk::Pipeline pipeline) {
    PreRecordBoilerplate;
    getCmd().bindPipeline(pipelineBindPoint, pipeline);
    return *this;
  };

  WorkBatch& WorkBatch::bindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const vk::Buffer * pBuffers,
					  const vk::DeviceSize * pOffsets) {
    PreRecordBoilerplate;
    getCmd().bindVertexBuffers(firstBinding, bindingCount, pBuffers, pOffsets);
    return *this;
  };

  WorkBatch& WorkBatch::draw(size_t vCnt, size_t firstVert, size_t instanceCnt, size_t firstInstance) {
    PreRecordBoilerplate;
    getCmd().draw(vCnt, instanceCnt, firstVert, firstInstance);
    return *this;
  };

};
