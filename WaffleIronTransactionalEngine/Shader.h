#pragma once

class Shader : public WITE::Shader {
public:
  typedef struct instance_st {
    std::unique_ptr<std::shared_ptr<class WITE::ShaderResource>[]> resources = std::nullptr_t();
    VkDescriptorSet descSet;
    bool inited = false;
  } Instance;
  struct imageData {
    uint32_t width, height;
    uint64_t format;
  };
  //Shader(const char* filepathWildcard, struct WITE::Shader::resourceLayoutEntry*, size_t resources);
  Shader(const char** filepath, size_t files, struct WITE::Shader::resourceLayoutEntry*, size_t resources, WITE::renderLayerMask appliesToLayers = ~0);
  ~Shader();
  void render(WITE::Queue::ExecutionPlan* ep, glm::dmat4 projection, VkRenderPass rp);
  void ensureResources(GPU*);
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
    VkDescriptorPool growDescPool(GPU*);
    //std::unique_ptr<std::map<VkRenderPass, std::shared_ptr<struct rpResources>>> rpRes;
    std::map<VkRenderPass, VkPipeline> pipeline;//both key and value are opaque points, so just ints with extra steps
  };
  VkPipeline makePipeForRP(VkRenderPass rp, GPU* gpu);
  size_t subshaderCount, resourcesPerInstance;
  WITE::GPUResource<shaderGpuResources> resources;
  struct subshader_t* subshaders;//TODO free
  struct WITE::Shader::resourceLayoutEntry* resourceLayout;//TODO free
  std::vector<class Renderer*> renderers[MAX_RENDER_LAYERS];
  WITE::SyncLock lock;
  std::unique_ptr<Instance> makeResources(GPU*);
  std::unique_ptr<struct shaderGpuResources> makeDescriptors(GPU*);
  static std::vector<Shader*> allShaders;
  friend class Renderer;
};

