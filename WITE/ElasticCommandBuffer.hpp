#include "Vulkan.hpp"

namespace WITE::GPU {

  class Queue;
  class Gpu;

  class ElasticCommandBuffer {
  private:
    vk::CommandBuffer cmd;
    Queue* owner;
    ElasticCommandBuffer(Queue* owner);
    friend class Queue;
  public:
    ~ElasticCommandBuffer();
    ElasticCommandBuffer(const ElasticCommandBuffer&) = delete;
    ElasticCommandBuffer(ElasticCommandBuffer&);
    ElasticCommandBuffer() : owner(NULL) {};
    inline operator vk::CommandBuffer() { return cmd; };
    inline vk::CommandBuffer* operator->() { return &cmd; };
    Gpu* getGpu();
  };

}
