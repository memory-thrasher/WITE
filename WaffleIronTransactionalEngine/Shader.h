#pragma once

#include "stdafx.h"
#include "Export.h"
#include "VBackedBuffer.h"
#include "Object.h"

class Shader : WITE::Shader {
public:
  typedef struct {
    std::unique_ptr<std::shared_ptr<ShaderResource>[]> resources;
    VkDescriptorSet descSet;
    bool inited;
  } Instance;
  struct resourceLayoutEntry {
    size_t type, stage, perInstance;
    void* moreData;
  };
  struct imageData {
    uint32_t width, height;
    uint64_t format;
  };
  Shader(const char* filepathWildcard, struct resourceLayoutEntry*, size_t resources);
  Shader(const char** filepath, size_t files, struct resourceLayoutEntry*, size_t resources);
  ~Shader();
  void render(VkCommandBuffer cmd, renderLayerMask layers, glm::mat4d projection, GPU* gpu);
  void ensureResources(GPU*);
  static void renderAll(VkCommandBuffer ep, renderLayerMask layers, glm::mat4d projection, GPU* gpu);
private:
  struct subshader_t {
    VkShaderModuleCreateInfo moduleInfo;//modulecreateflags, codesize, code
    const char* filepath;
    VkShaderStageFlagBits stageBit;
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
    void growDescPool(GPU*);
    //TODO per-camera struct for pipe and friends
  };
  void makePipeForRP(VkRenderPass rp, GPU* gpu, VkPipeline* out);
  GPUResource<shaderGpuResources> resources;
  struct subshader_t* subshaders;
  size_t subshaderCount, resourcesPerInstance;
  struct resourceLayoutEntry* resourceLayout;
  std::vector<class Renderer*> renderers[MAX_RENDER_LAYERS];
  SyncLock lock;
  std::unique_ptr<Instance> makeResources(GPU*);
  std::unique_ptr<struct shaderGpuResources> makeDescriptors(GPU*);
  static std::vector<std::shared_ptr<class Shader>> allShaders;
  friend class Renderer;
};

