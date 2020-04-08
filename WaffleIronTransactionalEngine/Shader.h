#pragma once

#include "stdafx.h"
#include "Export.h"
#include "BackedBuffer.h"
#include "GPU.h"
#include "Queue.h"

class Shader : public WITE::Shader {
public:
  typedef struct {
    std::unique_ptr<std::shared_ptr<class WITE::ShaderResource>[]> resources;
    VkDescriptorSet descSet;
    bool inited;
  } Instance;
  struct imageData {
    uint32_t width, height;
    uint64_t format;
  };
  Shader(const char* filepathWildcard, struct WITE::Shader::resourceLayoutEntry*, size_t resources);
  Shader(const char** filepath, size_t files, struct WITE::Shader::resourceLayoutEntry*, size_t resources);
  ~Shader();
  void render(Queue::ExecutionPlan* ep, WITE::renderLayerMask layers, glm::dmat4 projection, GPU* gpu, VkRenderPass rp);
  void ensureResources(GPU*);
  static void renderAll(Queue::ExecutionPlan* ep, WITE::renderLayerMask layers, glm::dmat4 projection, GPU* gpu, VkRenderPass rp);
private:
  static constexpr VkVertexInputBindingDescription viBinding = { 0, FLOAT_BYTES, VK_VERTEX_INPUT_RATE_VERTEX };
  static constexpr VkVertexInputAttributeDescription viAttributes[2] =
    { { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }, { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12 } };
  struct subshader_t {
    VkShaderModuleCreateInfo moduleInfo;//modulecreateflags, codesize, code
    const char* filepath;
    VkShaderStageFlagBits stageBit;
  };
  struct rpResources {
    VkPipeline pipeline;
  };
  struct shaderGpuResources {
    std::unique_ptr<VkPipelineShaderStageCreateInfo[]> stageInfos;
    std::unique_ptr<VkDescriptorSetLayoutBinding[]> layoutBindings;
    VkDescriptorSetLayoutCreateInfo descLayoutInfo;
    VkDescriptorSetLayout descLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    VkPipelineLayout pipelineLayout;
    std::vector<VkDescriptorPool> descPools;
    std::unique_ptr<VkDescriptorPoolSize[]> descPoolSizes;
    VkDescriptorPoolCreateInfo descPoolInfo;
    VkDescriptorSetAllocateInfo descAllocInfo;//qazi-template, set descPool before use
    VkDescriptorPool growDescPool(GPU*);
    std::unique_ptr<std::map<VkRenderPass, std::shared_ptr<struct rpResources>>> rpRes;
  };
  void makePipeForRP(VkRenderPass rp, GPU* gpu, VkPipeline* out);
  GPUResource<shaderGpuResources> resources;
  struct subshader_t* subshaders;
  size_t subshaderCount, resourcesPerInstance;
  struct WITE::Shader::resourceLayoutEntry* resourceLayout;
  std::vector<class Renderer*> renderers[MAX_RENDER_LAYERS];
  WITE::SyncLock lock;
  std::unique_ptr<Instance> makeResources(GPU*);
  std::unique_ptr<struct shaderGpuResources> makeDescriptors(GPU*);
  static std::vector<std::shared_ptr<class Shader>> allShaders;
  friend class Renderer;
};

