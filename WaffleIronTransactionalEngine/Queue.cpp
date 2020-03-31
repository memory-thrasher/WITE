#include "stdafx.h"
#include "Queue.h"
#include "GPU.h"
#include "Export.h"

const VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL };

Queue::Queue(GPU* gpu, uint32_t family, uint32_t idx) : gpu(gpu), family(family),
							complexPlans(complexPlan::Initer::make(this, &makeComplexPlan)) {
  vkGetDeviceQueue(gpu->device, family, 0, &queue);
  VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
				       VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, family };
  CRASHIFFAIL(vkCreateCommandPool(gpu->device, &poolInfo, NULL, &cmdPool));
  bufInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
}

Queue::~Queue() {}

VkCommandBuffer Queue::makeCmd() {
  VkCommandBuffer ret;
  ScopeLock hold(&q->lock);
  CRASHIFFAIL(vkAllocateCommandBuffers(gpu->device, &bufInfo, &ret));
  return ret;
}

void Queue::destroyCmd(VkCommandBuffer buf) {
  vkDestroyBuffer(gpu->device, buf, NULL);
}

void Queue::submit(VkCommandBuffer* cmds, size_t cmdLen, VkFence fence) {
  ScopeLock hold(&lock);
  size_t count = this->cmds.listAll(&cmds);
  VkCommandBuffer queued[MAX_THREADS];
  for (size_t i = 0, j = 0;i < count;i++)
    if (cmds[i].flush())
      queued[j++] = cmds[i].cmd;
  if (count) {
    VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL,
			    count, queued, 0, NULL };
    CRASHIFFAIL(vkQueueSubmit(queue, 1, &submit, fence));
  }
}

bool Queue::supports(VkSurfaceKHR* surface) {
  VkBool32 compatible;
  vkGetPhysicalDeviceSurfaceSupportKHR(gpu->phys, q->family, *surface, &compatible);
  return compatible;
}

ExecutionPlan* Queue::getComplexPlan() {
  return complexPlans.get();
}

void Queue::makeComplexPlan(ExecutionPlan* out) {
  new(out)ExecutionPlan(this);
}

size_t Queue::ExecutionPlan::beginInternal() {
  SerialQueue* sq;
  if(!hold) hold = std::make_unique(&queue->lock);
  if(headSems.size()) vkEndCommandBuffer(allocated[head].cmd);
  allocated.ensureCapacity(allocIdx+1);
  while(allocIdx >= allocated.size()) {
    allocated.emplace_back();
    allSubmits.emplace_back();
  }
  sq = allocated[allocIdx];
  if(!sq->cmd) {
    sq->cmd = queue->makeCmd();
    VkCreateSemaphore(queue->gpu->device, &SEMAPHORE_CREATE_INFO, NULL, &sq->completionSemaphore);
  }
  allSubmits[allocIdx] = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &cmd, 1, &sq->completionSemaphore };
  vkBeginCommandBuffer(sq->cmd, &CMDBUFFER_BEGIN_INFO);
  //??  vkResetCommandBuffer(&sq->cmd, 0);
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
  if(h) {
    h--;
    sq = allSubmits[i];
    sq->waitSemaphoreCount = 1;
    sq->waitSemaphores = &allocated[head]->completionSemaphore;
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
  si = allSubmits[i];
  si->waitSemaphoreCount = h;
  sq->waitSemaphores = (VkSemaphore*)realloc((void*)sq->waitSemaphores, h * sizeof(VkSemaphore));
  memcpy((void*)sq->waitedSemaphores, (void*)headSems.data(), h * sizeof(VkSemaphore));
  si->waitSemaphores = sq->waitedSemaphores;
  head = i;
  headSems.clear();
  headSems.push_back(sq->completionSemaphore);
}

void Queue::ExecutionPlan::queueWaitForSemaphore(VkSemaphore sem) {
  headSems.push_back(sem);
}

void Queue::ExecutionPlan::getStateSemaphores(VkSemaphore* outSems, size_t* outSemLen) {
  size_t hs = headSems.size();
  if(outSems) {
    if(*outSemLen < hs) hs = *outSemLen;
    else *outSemsLen = hs;
    memcpy((void*)outSems, outSems.data(), hs * sizeof(VkSemaphore));
  }
}

void Queue::ExecutionPlan::submit() {
  size_t hs = headSems.size();
  if(!hs) return;
  vkEndCommandBuffer(allocated[head].cmd);
  activeRender = true;
  VkQueueSubmit(queue->queue, allocIdx, allSubmits.data(), fence);
  headSems.clear();
  allSubmits.clear();
  allocIdx = head = 0;//TODO audit multi-cam issue with reuse of not yet executed cmds
}

bool Queue::ExecutionPlan::isRunning() {
  if(activeRender) {
    VkResult res = vkGetFenceStatus(queue->gpu->device, fence);
    switch(res) {
    case VK_SUCCESS:
      activeRender = false;
      hold.reset();
      break;
    case VK_ERROR_DEVICE_LOST:
    default:
      hold.reset();
      CRASHRET(false);
      break;
    case VK_NOT_READY: break;
    }
  }
  return activeRender;
}

VkCommandBuffer Queue::ExecutionPlan::getActive() {
  return allocated[head].cmd;
}

Queue::ExecutionPlan::~ExecutionPlan() {
  auto sqEnd = allocated.end();
  for(auto sq = allocated.begin();sq != sqEnd;sq++) {
    if(sq->waitedSemaphores) free(sq->waitedSemaphores);
    VkDestroySemaphore(queue->gpu->device, sq->completionSemaphore, NULL);
    //command buffers are handled by the command pool, which is allocated at the queue level atm
  }
  VkDestroyFence(queue->gpu->device, fence, NULL);
}

