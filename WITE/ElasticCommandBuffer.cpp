#include "ElasticCommandBuffer.hpp"
#include "Queue.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  ElasticCommandBuffer::ElasticCommandBuffer(Queue* q, uint64_t f) : cmd(q->getNext()), owner(q), frame(f) {};

  ElasticCommandBuffer::~ElasticCommandBuffer() {
    if(owner)
      owner->submit(cmd, frame);
    for(size_t i = 0;children[i];i++)
      delete children[i];
  };

  void ElasticCommandBuffer::addDependency(Semaphore* sem) {
    cmd->waits.insert(sem);
  };

  void ElasticCommandBuffer::addSignal(Semaphore* sem) {
    cmd->signals.insert(sem);
  };

  ElasticCommandBuffer& ElasticCommandBuffer::getOfType(QueueType qt) {
    if(!owner || supports(qt)) return *this;
    size_t i = 0;
    while(children[i] && !children[i]->supports(qt)) i++;
    if(children[i]) return *children[i];
    children[i] = new ElasticCommandBuffer(owner->getGpu()->getQueue(qt), frame);
    return *children[i];
  };

}

