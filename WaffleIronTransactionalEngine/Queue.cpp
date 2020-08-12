#include "stdafx.h"
#include "Queue.h"
#include "GPU.h"
#include "Export.h"
#include "Debugger.h"
#include "Thread.h"

const VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL };

Queue::Queue(GPU* gpu, uint32_t family, uint32_t idx) :
  gpu(gpu), family(family),
  complexPlans(decltype(complexPlans)::Initer_F::make<Queue>(this, &Queue::makeComplexPlan)) {
  vkGetDeviceQueue(gpu->device, family, 0, &queue);
  VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
				       VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, family };
  CRASHIFFAIL(vkCreateCommandPool(gpu->device, &poolInfo, NULL, &cmdPool));
  bufInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
}

Queue::~Queue() {}

VkCommandBuffer Queue::makeCmd() {
  VkCommandBuffer ret;
  WITE::ScopeLock hold(&lock);
  LOG("Creating %d-th vkCmd\n", cmdCount);
  CRASHIFFAIL(vkAllocateCommandBuffers(gpu->device, &bufInfo, &ret), NULL);
  Debugger::setObjectName(gpu, VK_OBJECT_TYPE_COMMAND_BUFFER, ret, "Command buffer %d for thread %d of queue of family %d of gpu %d", cmdCount, WITE::Thread::getCurrentTid(), family, gpu->idx);
  cmdCount++;
  return ret;
}

void Queue::destroyCmd(VkCommandBuffer doomed) {
}

void Queue::submit(VkCommandBuffer* cmds, size_t cmdLen, VkFence fence) {
  WITE::ScopeLock hold(&lock);
  if (cmdLen) {
    VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, (uint32_t)cmdLen, cmds, 0, NULL };
    CRASHIFFAIL(vkQueueSubmit(queue, 1, &submit, fence));
  }
}

bool Queue::supports(VkSurfaceKHR* surface) {
  VkBool32 compatible;
  vkGetPhysicalDeviceSurfaceSupportKHR(gpu->phys, family, *surface, &compatible);
  return compatible;
}

std::shared_ptr<Queue::ExecutionPlan> Queue::getComplexPlan() {
  return complexPlans.get();
}

std::shared_ptr<Queue::ExecutionPlan> Queue::makeComplexPlan() {
  return std::make_shared<ExecutionPlan, Queue*>(this);
}

Queue::ExecutionPlan::ExecutionPlan(Queue* master) : queue(master) {
  VkFenceCreateInfo fenceInfo;
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.pNext = NULL;
  fenceInfo.flags = 0;
  CRASHIFFAIL(vkCreateFence(master->gpu->device, &fenceInfo, NULL, &fence));
  Debugger::setObjectName(master->gpu, VK_OBJECT_TYPE_FENCE, fence, "Fence for thread %d of queue of family %d of gpu %d", WITE::Thread::getCurrentTid(), master->family, master->gpu->idx);
};

size_t Queue::ExecutionPlan::beginInternal() {
  SerialQueue* sq;
  if(allocIdx) vkEndCommandBuffer(allocated[head]->cmd);//FIXME broken by queueWaitForSemaphore
  if(allocIdx >= allocated.size()) {
    sq = new SerialQueue();
    allocated.push_back(sq);
    allSubmits.emplace_back();
    sq->cmd = queue->makeCmd();
    vkCreateSemaphore(queue->gpu->device, &SEMAPHORE_CREATE_INFO, NULL, &sq->completionSemaphore);
    Debugger::setObjectName(queue->gpu, VK_OBJECT_TYPE_SEMAPHORE, sq->completionSemaphore, "Semaphore for serial queue %d for execution plan for thread %d of queue of family %d of gpu %d", allocIdx, WITE::Thread::getCurrentTid(), queue->family, queue->gpu->idx);
  } else
    sq = allocated[allocIdx];
  allSubmits[allocIdx] = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &sq->cmd, 1, &sq->completionSemaphore };
  CRASHIFFAIL(vkBeginCommandBuffer(sq->cmd, &CMDBUFFER_BEGIN_INFO), 0);
  //??  vkResetCommandBuffer(&sq->cmd, 0); (if so, cmd buffer info can be one time submit flag)
  return allocIdx++;
}

VkCommandBuffer Queue::ExecutionPlan::beginParallel() {
  size_t i = beginInternal();
  head = i;
  headSems.push_back(allocated[i]->completionSemaphore);
  return allocated[i]->cmd;
}

VkCommandBuffer Queue::ExecutionPlan::beginSerial() {
  size_t i = beginInternal(), h = headSems.size();
  VkSubmitInfo* si;
  SerialQueue* sq;
  if(h) {
    h--;
	sq = allocated[i];
    si = &allSubmits[i];
    si->waitSemaphoreCount = 1;
    *sq->waitedSemaphores = allocated[head]->completionSemaphore;
    *sq->waitedSem_PipeStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;//TODO more specific/flexible?
    si->pWaitSemaphores = sq->waitedSemaphores;
    si->pWaitDstStageMask = sq->waitedSem_PipeStages;
    headSems[h] = allocated[i]->completionSemaphore;
  } else//rare, first begin call should be beginParallel
    headSems.push_back(allocated[i]->completionSemaphore);
  head = i;
  return allocated[i]->cmd;
}

VkCommandBuffer Queue::ExecutionPlan::beginReduce() {//Note: VkSemaphore is an opaque handle.
  SerialQueue* sq;
  VkSubmitInfo* si;
  size_t i = beginInternal(), h = headSems.size();
  sq = allocated[i];
  si = &allSubmits[i];
  si->waitSemaphoreCount = h;
  if (h > MAX_QUEUE_REDUCE) CRASHRET(NULL);//TODO reduction tree
  memcpy((void*)sq->waitedSemaphores, (void*)headSems.data(), h * sizeof(VkSemaphore));
  for(size_t i = 0;i < h;i++)
    sq->waitedSem_PipeStages[i] = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;//TODO more specific/flexible?
  si->pWaitSemaphores = sq->waitedSemaphores;
  si->pWaitDstStageMask = sq->waitedSem_PipeStages;
  head = i;
  headSems.clear();
  headSems.push_back(sq->completionSemaphore);
  return allocated[i]->cmd;
}

void Queue::ExecutionPlan::queueWaitForSemaphore(VkSemaphore sem) {
  headSems.push_back(sem);
}

void Queue::ExecutionPlan::popStateSemaphores(std::vector<VkSemaphore>* out) {
  out->swap(headSems);
  headSems.clear();
}

void Queue::ExecutionPlan::submit() {
  if(!allocIdx) return;
  CRASHIFFAIL(vkEndCommandBuffer(allocated[head]->cmd));
#ifdef _DEBUG
  if(isRunning())
    CRASH("NYI: synchronous executions from the same EP attempted, but only enough resources for a single execution are allocated presently\n");
#endif
  CRASHIFFAIL(vkResetFences(queue->gpu->device, 1, &fence));
  activeRender = true;
  CRASHIFFAIL(vkQueueSubmit(queue->queue, allocIdx, allSubmits.data(), fence));
  allocIdx = head = 0;//TODO audit multi-cam issue with reuse of not yet executed cmds
}

bool Queue::ExecutionPlan::isRunning() {
  if(activeRender) {
    VkResult res = vkGetFenceStatus(queue->gpu->device, fence);
    switch(res) {
    case VK_SUCCESS:
      activeRender = false;
      break;
    case VK_ERROR_DEVICE_LOST:
    default:
      CRASHRET(false);
      break;
    case VK_NOT_READY: break;
    }
  }
  return activeRender;
}

VkCommandBuffer Queue::ExecutionPlan::getActive() {
  return allocated[head]->cmd;
}

Queue::ExecutionPlan::~ExecutionPlan() {
  auto sqEnd = allocated.end();
  for(auto sq = allocated.begin();sq != sqEnd;sq++) {
    vkDestroySemaphore(queue->gpu->device, (*sq)->completionSemaphore, NULL);
	if (*sq) free(*sq);
    //command buffers are handled by the command pool, which is allocated at the queue level atm
  }
  vkDestroyFence(queue->gpu->device, fence, NULL);
}

