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
    inline vk::CommandBuffer operator vk::CommandBuffer() { return cmd; };
    Gpu* getGpu();
  };

}
