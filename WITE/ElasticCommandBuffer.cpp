#include "ElasticCommandBuffer.hpp"
#include "Queue.hpp"

namespace WITE::GPU {

  ElasticCommandBuffer::ElasticCommandBuffer(Queue* q) : owner(q), cmd(q->getNext()) {};

  ElasticCommandBuffer::~ElasticCommandBuffer() {
    if(q)
      q->submit(cmd);
  };

  ElasticCommandBuffer::ElasticCommandBuffer(ElasticCommandBuffer& o) : owner(o.owner), cmd(o.cmd) {
    o.owner = NULL;
  };

  Gpu* ElasticCommandBuffer::getGpu() {
    return owner->getGpu();
  };

}

