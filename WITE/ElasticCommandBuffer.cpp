#include "ElasticCommandBuffer.hpp"
#include "Queue.hpp"

namespace WITE::GPU {

  ElasticCommandBuffer::ElasticCommandBuffer(Queue* q) : cmd(q->getNext()), owner(q) {};

  ElasticCommandBuffer::~ElasticCommandBuffer() {
    if(owner)
      owner->submit(this);
  };

  ElasticCommandBuffer::ElasticCommandBuffer(ElasticCommandBuffer& o) : cmd(o.cmd), owner(o.owner) {
    o.owner = NULL;
  };

  Gpu* ElasticCommandBuffer::getGpu() {
    return owner->getGpu();
  };

}

