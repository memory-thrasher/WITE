#include "WorkBatch.hpp"
#include "Queue.hpp"
#include "constants.hpp"
#include "Thread.hpp"

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
    cycle++;
    state = state_t::recording;
    // waiters.clear();
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
    cycle = batch->cycle;
    batch->getCurrent().gpu = gpuIdx;
  };

  WorkBatch::WorkBatch(logicalDeviceMask_t ldm) : WorkBatch(Gpu::getGpuFor(ldm)) {};

  WorkBatch::WorkBatch(Queue* q) : WorkBatch(q->getGpuIdx()) {
    get()->getCurrent().q = q;
  };

  void WorkBatch::init() {//static
    auto loop = Platform::Thread::threadEntry_t_F::make(checkWorkerLoop);
    for(size_t i = 0;i < PROMISE_THREAD_COUNT;i++) {
      PlatformThread::spawnThread(loop);
    }
  };

  void WorkerBatch::checkWorkerLoop() {//static
    while(Gpu::running) {
      WorkBatchResources* wbr = allBatches.getAny();//resilient semi-ordered iteration
      if(!wbr) continue;//list empty: busy wait
      wbr->check();
    }
  };

  void WorkBatch::check() {
    auto b = get();
    if(!b) return;
    Util::AdvancedScopeLock lock(&b->mutex, 0);
    if(!lock.isHeld()) return;//if another thread has this locked, don't check anything but also don't wait
    if(b.cycle != cycle) return;
    switch(b->state) {
    case recording: return;
    case pending:
      if(!b->canStart()) return;//TODO eventually allow b to start when a prerequisite is in progress if a semaphore can be used
      b->state = state_t::running;
      b->nextStep = 0;
      if(!b->steps.empty())
	b->execute();
      break;
    case running:
      ASSERT_TRAP(b->nextStep > 0, "already in running state but first work step hasn't started");
      Work& currentStep = b->steps[b->nextStep - 1];
      if(currentStep.cpu || currentStep.completeSem.pending())
	break; //currentStep is not done. cpu callback is set to null when finished
      if(b->nextStep >= b->steps.size()) {//it's over
	b->state = state_t::done;
      } else {
	b->execute();
	break;
      }
    case done:
      //TODO proc waiters if any. Need deadlock resolution, see comment above mustHappenAfter
      b->reset();
      allBatches.free(b);
    };
  };

  void WorkBatchResources::execute() {
    do {
      ASSERT_TRAP(state == state_t::running, "attempted to execute on invalid state");
      ASSERT_TRAP(nextStep < steps.size(), "out of bounds");
      Work& w = steps[nextStep];
      if(w.cpu) {
	w.cpu(*this);
	w.cpu = NULL;
      }
      if(w.q && w.cmd) {
	waitedSemsStaging.clear();
	do {
	  w = steps[nextStep];
	  cmdStaging.clear();
	  signalSemsStaging.clear();
	  for(;nextStep < steps.size();nextStep++) {
	    Work& w2 = steps[nextStep];
	    if(w2.cpu || w2.q != w.q)
	      break;
	    cmdStaging.emplace_back(w2.cmd);
	    signalSemsStaging.push_back(w2.completeSem.prepareForBatchSubmit());
	  }
	  vk::SubmitInfo2 si { {},
	    waitedSemsStaging.size(), waitedSemsStaging.data(),
	    cmdStaging.size(), cmdStaging.data(),
	    signalSemsStaging.size(), signalSemsStaging.data()
	  };
	  w.q->submit(si);
	  //for next loop:
	  waitedSemsStaging.clear();
	  waitedSemsStaging.push_back(signalSemsStaging[signalSemsStaging.size()-1]);
	} while(nextStep < steps.size() && steps[nextStep].gpu == w.gpu && !steps[nextStep].cpu);
	break;
      }
      nextStep++;
    } while(nextStep < steps.count());
  };

  vk::CommandBuffer WorkBatchResources::getCmd(QueueType qt) {
    Work c = getCurrent();
    ASSERT_TRAP(~c.gpu, "get cmd called when no gpu has been chosen (need a thenOnGpu first)");
    if(c.q && c.q->supports(qt)) {
      [[likely]]
      if(!c.cmd)
	c.cmd = c.q->getNext();
      return c.cmd;
    }
    size_t gpu = c.gpu;
    if(c.q) {
      c = append();
      c.gpu = gpu;
    }
    c.q = Gpu::get(gpu).getQueue(qt);
    return c.cmd = q->getNext();
  };

  vk::CommandBuffer WorkBatchResources::getCmd() {//used for commands that do not care which queue is used
    Work c = getCurrent();
    ASSERT_TRAP(~c.gpu, "get cmd called when no gpu has been chosen (need a thenOnGpu first)");
    if(c.q) {
      [[likely]]
      if(!c.cmd) [[unlikely]]
	c.cmd = c.q->getNext();
    } else {
      c.q = Gpu::get(gpu).getQueue(QueueType::eTransfer);
      c.cmd = q->getNext();
    }
    return c.cmd;
  };

  #define PreRecordBoilerplate \
    auto b = get(); \
    ASSERT_TRAP(b, "attempted to append to very stale batch"); \
    Util::AdvancedScopeLock lock(&b->mutex); \
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
    if(!Gpu::ldmContains(ldm, current->gpu))
      thenOnGpu(Gpu::getGpuFor(ldm));
    return *this;
  };

  const WorkBatch WorkBatch::submit() {
    PreRecordBoilerplate;//submit requires it be in the recording state too
    ASSERT_TRAP(!b->isRecordingRP, "attempted to submit a command buffer while a render pass is being recorded (missing endRenderPass)");
    state = state_t::pending;
    //potential future enhancement: check if all prerequisits are also on this gpu and if so submit this now linked by semaphore
    lock.release();
    check();//see if this can begin and if so, do it
    return *this;
  };

  /*
    race condition:
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

  bool WorkBatch::isDone() {
    auto b = get();
    if(!b)
      return true;
    Util::AdvancedScopeLock lock(&b->mutex, 0);
    if(!lock.isHeld())
      return false;
    return b->state == state_t::done || b->cycle != cycle;
    //cycle check covers race condition if b recycled just now
  };

  bool WorkBatch::isSubmitted() {
    auto b = get();
    if(!b)
      return true;
    //NOTE dirty read, may fail sporadically
    return b->state != state_t::recording || b->cycle != cycle;
  };

  void WorkBatch::putImageInLayout(ImageBase* img, vk::ImageLayout l, QueueType qt,
				   vk::AccessFlags2 a, vk::PipelineStageFlags2 s) {
    putImageInLayout(img, l, getGpu().getQueue(qt).family, a, s);
  };

  void WorkBatch::putImageInLayout(ImageBase* img, vk::ImageLayout l, uint32_t qFam,
				   vk::AccessFlags2 a, vk::PipelineStageFlags2 s) {
    img->accessStateTracker.get(getGpuIdx()).inState({ l, a, s, qFam });
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
    size_t ret = b->getCurrent.gpu;
    ASSERT_TRAP(~ret, "requested gpu when one hasn't been chosen (need a thenOnGpu first)");
    return ret;
  };

  //begin duplication of vk::cmd functions

  WorkBatch& WorkBatch::pipelineBarrier2(vk::DependencyInfo* di) {
    PreRecordBoilerplate;
    getCmd().pipelineBarrier2(di);
    return *this;
  };

  void WorkBatch::batchDistributeAcrossLdm(size_t srcGpu, size_t cntAll, ImageBase** srcs, ImageBase** stagings, void** ram) {//static
    const size_t maxPerBatch = 64;
    vk::ImageMemoryBarrier mbs[maxPerBatch * 2];
    vk::DependencyInfo di;
    di.pImageMemoryBarriers = mb;
    size_t srcGpu = getGpuIdx();
    //NOTE: neither image copy nor memory barrier need any specific queue type, so use whatever owns the first image
    uint32_t q = srcs[i]->accessStateTracker.getRef(srcGpu).get().queueFam;

#define appendTransition(gpu, img, layout, access, stage) {		\
      ImageBase::accessState gnu = { vk::ImageLayout::e ##layout, vk::AccessFlagBits2::e ##access, \
	vk::PipelineStageFlagBits2::e ##stage, q			\
      }, old = img->accessStateTracker.getRef(gpu).onExternalChange(gnu, *this); \
      if(old.layout != gnu.layout || old.queueFam != gnu.queueFam)	\
	mbs[mbCnt++] = { old.stageMask, old.accessMask, gnu.stagemask, gnu.accessMask, \
	  gnu.layout, gnu.layout, old.queueFam, gnu.queueFam,		\
	  img->getVkImage(gpu), img->getAllInclusiveSubresource() };	\
    }
#define appendDiscard(gpu, img, layout, access, stage) {		\
      ImageBase::accessState gnu = { vk::ImageLayout::e ##layout, vk::AccessFlagBits2::e ##access, \
	vk::PipelineStageFlagBits2::e ##stage, q			\
      }, old = img->accessStateTracker.getRef(gpu).onExternalChange(gnu, *this); \
      mbs[mbCnt++] = { old.stageMask, old.accessMask, gnu.stagemask, gnu.accessMask, \
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
      size_t mbCnt = 0;
      WorkBatch srcCmd(srcGpu);
      //host access requires linear tiling and general layout. General layout allows all operation.
      for(size_t i = 0;i < cnt;i++) {
	ASSERT_TRAP(((stagings && stagings[i] ? stagings[i] : srcs[i])->slotData.usage & GpuResource::MUSAGE_ANY_HOST) ==
		    GpuResource::MUSAGE_ANY_HOST,
		    "Attempted to batch distribute an image that is not host read/writable (copy requires linear tiling)");
	if(stagings && stagings[i]) {
	  appendDiscard(srcGpu, stagings[i], General, TransferWrite | vk::AccessFlagBits2::eHostRead,
			Copy | vk::PipelineStageFlagBits2::eHost);
	  appendTransition(srcGpu, srcs[i], TransferSrcOptimal, TransferRead, Copy);
	} else {
	  appendTransition(srcGpu, srcs[i], General, HostRead, Host);
	}
      }
      doBarrier(srcCmd);
      for(size_t i = 0;i < cnt;i++)
	if(stagings && stagings[i])
	  srcCmd.copyImage(srcs[i], stagings[i]);
      for(size_t i = 0;i < cnt;i++)
	srcCmd.then(thenCB_F::make((stagings && stagings[i] ? stagings[i] : srcs[i])->mem.getPtr(srcGpu), ram[i], VRam::read));
      srcCmd.submit();
      //note: splitting to one cmd per gpu so they can be parallel
      for(size_t gpu = 0;gpu < Gpu::getGpuCount();gpu++) {
	if(gpu == srcGpu || !Gpu::ldmContains(src[i]->slotData.logicalDeviceMask, gpu)) continue;
	WorkBatch cmd(gpu);
	cmd.mustHappenAfter(srcCmd);
	for(size_t i = 0;i < cnt;i++) {
	  if(stagings && stagings[i]) {
	    ASSERT_TRAP(stagings[i]->getMemSize(srcGpu) == stagings[i]->getMemSize(gpu),
			"Ram size of same image format/dimensions is different on different gpus when attempting to copy");
	    appendDiscard(gpu, stagings[i], General, TransferRead | vk::AccessFlagBits2::eHostWrite,
			  Copy | vk::PipelineStageFlagBits2::eHost);
	    appendDiscard(gpu, srcs[i], TransferDstOptimal, TransferRead, Copy);
	  } else {
	    ASSERT_TRAP(srcs[i]->getMemSize(srcGpu) == srcs[i]->getMemSize(gpu),
			"Ram size of same image format/dimensions is different on different gpus when attempting to copy");
	    appendDiscard(gpu, srcs[i], Preinitialized, HostWrite, Host);
	  }
	}
	doBarrier(cmd);
	for(size_t i = 0;i < cnt;i++)
	  cmd.then(thenCB_F::make((stagings && stagings[i] ? stagings[i] : srcs[i])->mem.getPtr(gpu), ram[i], VRam::write));
	cmd.thenOnGpu(gpu);
	for(size_t i = 0;i < cnt;i++)
	  if(stagings && stagings[i])
	    cmd.copyImage(stagings[i], srcs[i]);
	cmd.submit();
      }
      cntAll -= cnt;
      srcs += cnt;
      dsts += cnt;
      if(stagings) stagings += cnt;
    }
    return *this;
  }

  WorkBatch& WorkBatch::beginRenderPass(const renderInfo* ri) {
    PreRecordBoilerplate;
    ASSERT_TRAP(!b->isRecordingRP, "Duplicate RP begin");
    b->isRecordingRP = true;
    b->currentRP = *ri;
    auto cmd = getCmd(QueueType::eGraphics);
    size_t gpuIdx = getGpuIdx();
    vk::ImageMemoryBarrier mbs[renderInfo::MAX_RESOURCES];
    vk::DependencyInfo di;
    di.pImageMemoryBarriers = mb;
    for(size_t i = 0;i < b->currentRP.resourceCount;i++)
      if(b->currentRP.initialLayouts[i] != vk::ImageLayout::eUndefined)
	appendTransition(gpuIdx, b->currentRP.resources[i], b->currentRP.initialLayouts[i], TopOfPipe, AllGraphics);
    doBarrier(cmd);
    cmd.beginRenderPass(&b->currentRP.rpbi);
  };
#undefine appendTransition
#undefine appendDiscard

  WorkBatch& WorkBatch::endRenderPass() {
    PreRecordBoilerplate;
    ASSERT_TRAP(b->isRecordingRP, "RP end when not in an RP");
    b->isRecordingRP = false;
    size_t gpuIdx = getGpuIdx();
    for(size_t i = 0;i < b->currentRP.resourceCount;i++)
      b->currentRP.resources[i]->accessStateTracker.getRef(gpuIdx).onExternalChange({ b->currentRp.finalLayouts[i],
	  vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
	  vk::PipelineStageFlagBits2::eBottomOfPipe,
	  b->getCurrent().q->family
	});
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
    ASSERT_TRAP(src->getVkSize3D() == dst->getVkSize3D(), "copy size mismatch");
    vk::ImageCopy r(src->getAllSubresourceLayers(), {}, dst->getAllSubresourceLayers(), {}, src->getVkSize3D());
    cmd.copyImage(src->getVkImage(gpuIdx), srcLayout, dst->getVkImage(gpuIdx), dstLayout, 1, &r);
  };

  WorkBatch& WorkBatch::copyImage(ImageBase* src, vk::Image dst, vk::ImageLayout postLayout) {
    PreRecordBoilerplate;
    auto& w = b->getCurrent();
    auto cmd = getCmd();
    size_t gpuIdx = getGpuIdx();
    auto srcLayout = src->accessStateTracker.getRef(gpuIdx).get().layout;
    if(srcLayout != vk::ImageLayout::eGeneral) srcLayout = vk::ImageLayout::eTransferSrcOptimal;
    putImageInLayout(src, srcLayout, currentQFam(), vk::AccessFlagBits2::eTransferRead, vk::PipelineStageFlagBits2::eCopy);
    //just assume they're the same size
    auto tempLayout = postLayout == vk::ImageLayout::eGeneral ? postLayout : vk::ImageLayout::eTransferDstOptimal;
    discardImage(dst, tempLayout);
    vk::BufferCopy r(src->getAllSubresourceLayers(), {}, src->getAllSubresourceLayers(), {}, src->getVkSize3D());
    cmd.copyImage(src->getVkImage(gpuIdx), srcLayout, dst, tempLayout, 1, &r);
    quickImageBarrier(cmd, dst, tempLayout, postLayout);
  };

  static void WorkBatch::quickImageBarrier(vk::CommandBuffer cmd, vk::Image img, vk::ImageLayout pre, vk::ImageLayout post) {
    vk::ImageMemoryBarrier2 mb {
      vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone,
      vk::PipelineStageFlagBits2::eAllCommands, , vk::AccessFlagBits2::eNone,
      pre, post, 0, 0, img, { vk::ImageAspectBits::eColor, 0, 1, 0, 1 }
    };
    vk::DependencyInfo di;
    di.pImageMemoryBarriers = &mb;
    di.imageMemoryBarrierCount = 1;
    cmd.pipelineBarrier2(&di);
  };

  static WorkBatch& WorkBatch::discardImage(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout post) {
    quickImageBarrier(cmd, image, vk::ImageLayout::eUndefined, post);
  };

  vk::Fence WorkBatchResources::useFence() {
    auto& c = getCurrent();
    if(c.fence)
      c = append();
    //TODO recycling pool of fences pergpu
    return ret;
  };

  WorkBatch& WorkBatch::present(ImageBase*, vk::Image* swapImages, vk::SwapchainKHR swap) {
    PreRecordBoilerplate;
    //TODO so, acquiring the swapchain image has to signal a semaphore or fence. That should happen in a then callback.
    //So need infrastructure to have an optional signal/fence for non-command buffered work that signals.
    #error
    cmd->copyImage(img, &?);
    }
  };

};
