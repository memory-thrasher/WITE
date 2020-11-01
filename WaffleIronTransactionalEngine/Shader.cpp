#include "Internal.h"

const struct extensionMeaning {
  const char* suffix;
  const VkShaderStageFlagBits stageBit;
} SUFFIXES[]{ { "vert.spv", VK_SHADER_STAGE_VERTEX_BIT },{ "frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT } };

std::vector<Shader*> Shader::allShaders;

Shader::Shader(const char** paths, size_t pathc, struct WITE::Shader::resourceLayoutEntry* res, size_t resc) :
  subshaderCount(pathc),
  resourcesPerInstance(resc),
  resources(decltype(resources)::constructor_F::make<Shader>(this, &Shader::makeDescriptors)),
  subshaders(new struct subshader_t[pathc]),
  resourceLayout(new struct resourceLayoutEntry[resc]) {
  size_t i, j;
  for (i = 0;i < pathc;i++) {
    for (j = 0;j < sizeof(SUFFIXES) / sizeof(extensionMeaning) && !WITE::endsWith(paths[i], SUFFIXES[j].suffix);j++) {}
    subshaders[i].stageBit = SUFFIXES[j].stageBit;
    subshaders[i].filepath = paths[i];
    subshaders[i].moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, 0, NULL };
    if (!WITE::loadFile(subshaders[i].filepath, subshaders[i].moduleInfo.codeSize, (const unsigned char**)(&subshaders[i].moduleInfo.pCode)))
      CRASH("Failed to load shader file: %s\n", subshaders[i].filepath);
  }//TODO mmap shader file?
  memcpy(resourceLayout, res, sizeof(struct resourceLayoutEntry) * resc);
  allShaders.push_back(this);
}

// Shader::Shader(const char* filepathwildcard, struct WITE::Shader::resourceLayoutEntry* res, size_t resources) :
//   Shader(filepathwildcard, res, resources) {}//TODO actual wildcard

Shader::~Shader() {
}

void Shader::ensureResources(GPU* dev) {
  resources.get(dev);
}

const VkPipelineCacheCreateInfo EMPTY_CACHE = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, 0, 0, 0, 0 };

std::unique_ptr<struct Shader::shaderGpuResources> Shader::makeDescriptors(GPU* dev) {
  size_t i;
  auto ret = std::make_unique<struct Shader::shaderGpuResources>();
  ret->stageInfos = std::make_unique<VkPipelineShaderStageCreateInfo[]>(subshaderCount);
  for (i = 0;i < subshaderCount;i++) {
    ret->stageInfos[i] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, VK_NULL_HANDLE, 0,
			   subshaders[i].stageBit, VK_NULL_HANDLE, "main", VK_NULL_HANDLE };
    CRASHIFFAIL(vkCreateShaderModule(dev->device, &subshaders[i].moduleInfo, vkAlloc, &ret->stageInfos[i].module), NULL);
  }
  ret->layoutBindings = std::make_unique<VkDescriptorSetLayoutBinding[]>(resourcesPerInstance);
  ret->descPoolSizes = std::make_unique<VkDescriptorPoolSize[]>(resourcesPerInstance);
  for (i = 0;i < resourcesPerInstance;i++) {
    ret->layoutBindings[i] = { (uint32_t)i, (VkDescriptorType)resourceLayout[i].type, 1, (VkShaderStageFlags)resourceLayout[i].stage, VK_NULL_HANDLE };
    ret->descPoolSizes[i] = { (VkDescriptorType)resourceLayout[i].type, 1 };
  }
  ret->descLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, VK_NULL_HANDLE, 0, (uint32_t)resourcesPerInstance, ret->layoutBindings.get() };
  CRASHIFFAIL(vkCreateDescriptorSetLayout(dev->device, &ret->descLayoutInfo, vkAlloc, &ret->descLayout), VK_NULL_HANDLE);
  ret->pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, VK_NULL_HANDLE, 0, 1, &ret->descLayout, 0, VK_NULL_HANDLE };
  CRASHIFFAIL(vkCreatePipelineLayout(dev->device, &ret->pipelineLayoutInfo, vkAlloc, &ret->pipelineLayout), VK_NULL_HANDLE);
  ret->descPoolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, VK_NULL_HANDLE, 0, 1, (uint32_t)resourcesPerInstance, ret->descPoolSizes.get() };
  ret->descAllocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ret->descLayout };
  ret->growDescPool(dev);
  if(!dev->pipeCache)
    CRASHIFFAIL(vkCreatePipelineCache(dev->device, &EMPTY_CACHE, VK_NULL_HANDLE, &dev->pipeCache), NULL);//FIXME this -1 in GPU.cpp
  // ret->rpRes = std::make_unique<std::map<VkRenderPass, std::shared_ptr<struct rpResources>>>();
  return ret;
}

VkDescriptorPool Shader::shaderGpuResources::growDescPool(GPU* gpu) {
  size_t pools = descPools.size(), growth = 1ull << pools;
  VkDescriptorPool newPool;
  descPoolInfo.maxSets = growth;
  CRASHIFFAIL(vkCreateDescriptorPool(gpu->device, &descPoolInfo, VK_NULL_HANDLE, &newPool), VK_NULL_HANDLE);
  descPools.push_back(newPool);
  return newPool;
}

std::unique_ptr<Shader::Instance> Shader::makeResources(GPU* device) {
  std::unique_ptr<Instance> ret = std::make_unique<Instance>();
  VkExtent2D ve2;
  struct imageData* idata;
  ret->resources = std::make_unique<std::shared_ptr<class WITE::ShaderResource>[]>(resourcesPerInstance);
  for (size_t i = 0;i < resourcesPerInstance;i++) {
    if (!resourceLayout[i].perInstance) {
      ret->resources[i] = std::nullptr_t();
      continue;
    }
    switch (resourceLayout[i].type) {
    case SHADER_RESOURCE_UNIFORM:
      ret->resources[i] = std::make_shared<BackedBuffer>(device, reinterpret_cast<size_t>(resourceLayout[i].moreData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
      break;
    case SHADER_RESOURCE_SAMPLED_IMAGE:
      idata = static_cast<struct imageData*>(resourceLayout[i].moreData);
      ve2 = { idata->width, idata->height };
      ret->resources[i] = std::make_shared<BackedImage>(device, ve2, static_cast<VkFormat>(idata->format));
      break;
    default: CRASHRET(NULL); break;
    }
  }
  struct shaderGpuResources* gpuRes = resources.get(device);
  VkResult res;
  for(auto& pool : gpuRes->descPools) {
    gpuRes->descAllocInfo.descriptorPool = pool;
    res = vkAllocateDescriptorSets(device->device, &gpuRes->descAllocInfo, &ret->descSet);
    if(!res) break;
  }
  if(res) {
    gpuRes->descAllocInfo.descriptorPool = gpuRes->growDescPool(device);
    CRASHIFFAIL(vkAllocateDescriptorSets(device->device, &gpuRes->descAllocInfo, &ret->descSet), NULL);
  }
  ret->inited = false;
  return ret;
}

void Shader::makePipeForRP(VkRenderPass rp, size_t passIdx, GPU* gpu, VkPipeline* out) {
  struct shaderGpuResources* resources = this->resources.get(gpu);
  static VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
  static VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, VK_NULL_HANDLE, 0, 2, dynamicStateEnables };
  static VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, VK_NULL_HANDLE, 0, 1, &viBinding, 3, viAttributes };
  static VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, VK_NULL_HANDLE, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };
  static VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, VK_NULL_HANDLE, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f };
  static VkPipelineColorBlendAttachmentState attState = { 0, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xF };
  static VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, VK_NULL_HANDLE, 0, 0, VK_LOGIC_OP_NO_OP, 1, &attState, {1, 1, 1, 1} };
  static VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, VK_NULL_HANDLE, 0, 1, VK_NULL_HANDLE, 1, VK_NULL_HANDLE };
  static VkStencilOpState stencilOp = { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 };
  static VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, VK_NULL_HANDLE, 0, 1, 1, VK_COMPARE_OP_LESS_OR_EQUAL, 0, 0, stencilOp, stencilOp, 0, 0 };
  static VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, VK_NULL_HANDLE, 0, VK_SAMPLE_COUNT_1_BIT, 0, 0, VK_NULL_HANDLE, 0, 0 };
  static VkGraphicsPipelineCreateInfo pipeInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, VK_NULL_HANDLE, 0, 2, VK_NULL_HANDLE, &vi, &ia, VK_NULL_HANDLE, &vp, &rs, &ms, &ds, &cb, &dynamicState, resources->pipelineLayout, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0 };
  pipeInfo.stageCount = subshaderCount;
  pipeInfo.pStages = resources->stageInfos.get();
  pipeInfo.renderPass = rp;
  CRASHIFFAIL(vkCreateGraphicsPipelines(gpu->device, gpu->pipeCache, 1, &pipeInfo, VK_NULL_HANDLE, out));// &resources->rpRes.get()->at(rp)->pipeline
  pipeInfo.stageCount = 0;//don't leak handle to thread stack
  pipeInfo.pStages = VK_NULL_HANDLE;
  pipeInfo.renderPass = VK_NULL_HANDLE;
}

void Shader::render(std::shared_ptr<Queue::ExecutionPlan> ep, WITE::renderLayerMask layers, glm::dmat4 projection, GPU* gpu, VkRenderPass rp) {
  VkCommandBuffer cmd;
  struct shaderGpuResources* resources = this->resources.get(gpu);
  // auto rpRes = &*resources->rpRes;//map*
  // auto rprIter = rpRes->find(rp);//iterator
  // std::shared_ptr<struct rpResources> rpr;
  // if(rprIter != resources->rpRes->end()) {
  //   rpr = rprIter->second;
  // } else {
  //   rpr = std::make_shared<struct rpResources>();
  //   makePipeForRP(rp, gpu, &rpr->pipeline);
  //   rpRes->insert(std::pair<VkRenderPass, std::shared_ptr<struct rpResources>>(rp, rpr));
  // }
  size_t rpIdx = 0;//TODO rp index, assume one pass for now
  while(resources->perRP.size() <= rpIdx) resources->perRP.emplace_back();
  auto rpr = &resources->perRP[rpIdx];
  if(!rpr->pipeline) makePipeForRP(rp, rpIdx, gpu, &rpr->pipeline);
  cmd = ep->getActive();
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rpr->pipeline);
  for(WITE::renderLayerIdx rl = 0;rl < MAX_RENDER_LAYERS;rl++) {
    if(layers & (1ull << rl)) {
      auto end = renderers[rl].end();
      for(auto renderer = renderers[rl].begin();renderer != end;renderer++)
	(*renderer)->render(cmd, projection, gpu);
    }
  }
}

void Shader::renderAll(std::shared_ptr<Queue::ExecutionPlan> ep, WITE::renderLayerMask layers, glm::dmat4 projection, GPU* gpu, VkRenderPass rp) {
  auto end = allShaders.end();
  for(auto shader = allShaders.begin();shader != end;shader++)
    (*shader)->render(ep, layers, projection, gpu, rp);
}

WITE::Shader* WITE::Shader::make(const char** filepath, size_t files, struct WITE::Shader::resourceLayoutEntry* srles, size_t resources) {
  return new ::Shader(filepath, files, srles, resources);
}

// WITE::Shader* WITE::Shader::make(const char* filepathWildcard, struct WITE::Shader::resourceLayoutEntry* srles, size_t resources) {
//   return new ::Shader(filepathWildcard, srles, resources);
// }

