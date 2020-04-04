#include "stdafx.h"
#include "Shader.h"

const struct extensionMeaning {
  const char* suffix;
  const VkShaderStageFlagBits stageBit;
} SUFFIXES[]{ { "vert.spv", VK_SHADER_STAGE_VERTEX_BIT },{ "frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT } };

Shader::Shader(const char** paths, size_t pathc, struct resourceLayoutEntry* res, size_t resc) :
  subshaderCount(pathc),
  resources(CallbackFactory<std::unique_ptr<shaderGpuResources>, GPU*>::make<Shader>(this, Shader::makeDescriptors)),
  subshaders(new struct subshader_t[pathc]),
  resourcesPerInstance(resc),
  resourceLayout(new struct resourceLayoutEntry[resc]) {
  size_t i, j;
  for (i = 0;i < pathc;i++) {
    for (j = 0;j < sizeof(SUFFIXES) / sizeof(extensionMeaning) &&
	   !endsWith(paths[i], SUFFIXES[j].suffix);j++) {}
    subshaders[i].stageBit = SUFFIXES[j].stageBit;
    subshaders[i].filepath = paths[i];
    subshaders[i].moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, 0, NULL };
    if (!loadFile(subshaders[i].filepath, subshaders[i].moduleInfo.codeSize,
		  (const unsigned char**)(&subshaders[i].moduleInfo.pCode))) CRASH;
  }//TODO mmap shader file?
  memcpy(resourceLayout, res, sizeof(struct resourceLayoutEntry) * resc);
}

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
    ret->stageInfos[i] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
			   subshaders[i].stageBit, NULL, "main", NULL };
    CRASHIFFAIL(vkCreateShaderModule(dev->device, &subshaders[i].moduleInfo, vkAlloc,
				     &ret->stageInfos[i].module));
  }
  ret->layoutBindings = std::make_unique<VkDescriptorSetLayoutBinding[]>(resourcesPerInstance);
  ret->descPoolSizes = std::make_unique<VkDescriptorPoolSize[]>(resourcesPerInstance);
  for (i = 0;i < resourcesPerInstance;i++) {
    ret->layoutBindings[i] = { i, (VkDescriptorType)resourceLayout[i].type, 1, resourceLayout[i].stage, NULL };
    ret->descPoolSizes[i] = { (VkDescriptorType)resourceLayout[i].type, 1 };
  }
  ret->descLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0,
			  resourcesPerInstance, ret->layoutBindings.get() };
  CRASHIFFAIL(vkCreateDescriptorSetLayout(dev->device, &ret->descLayoutInfo, vkAlloc, &ret->descLayout));
  ret->pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0, 1,
			      &ret->descLayout, 0, NULL };
  CRASHIFFAIL(vkCreatePipelineLayout(dev->device, &ret->pipelineLayoutInfo, vkAlloc, &ret->pipelineLayout));
  ret->descPoolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL, 0, 1, 2, *ret->descPoolSizes };
  ret->descAllocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, NULL, NULL, 1, &ret->descLayout };
  ret->growDescPool();
  if(!dev->pipeCache)
    CRASHIFFAIL(vkCreatePipelineCache(dev->device, &EMPTY_CACHE, NULL, &dev->pipeCache));//FIXME this -1 in GPU.cpp
  return ret;
}

VkDescriptorPool Shader::shaderGpuResources::growDescPool(GPU* gpu) {
  size_t pools = descPools.size(), growth = pools == 0 ? 1  : pools == 1 ? 2 : descPools[pools-1].size() + descPools[pools-2].size();//fib
  VkDescriptorPool newPool;
  descPoolInfo.maxSets = growth;
  CRASHIFFAIL(vkCreateDescriptorPool(gpu->device, &descPoolInfo, NULL, &newPool));
  descPools.push_back(newPool);
  return newPool;
}

std::unique_ptr<Shader::Instance> Shader::makeResources(GPU* device) {
  std::unique_ptr<Instance> ret = std::make_unique<Instance>();
  ret->resources = std::make_unique<std::shared_ptr<ShaderResource>[]>(resourcesPerInstance);
  for (size_t i = 0;i < resourcesPerInstance;i++) {
    if (!resourceLayout[i].perInstance) continue;
    switch (resourceLayout[i].type) {
    case SHADER_RESOURCE_UNIFORM:
      ret->resources[i] = std::make_shared<BackedBuffer>(device, *static_cast<uint64_t*>(resourceLayout[i].moreData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
      break;
    case SHADER_RESOURCE_SAMPLED_IMAGE:
      struct imageData* idata = static_cast<struct imageData*>(resourceLayout[i].moreData);
      VkExtent2D ve2 = { idata->width, idata->height };
      ret->resources[i] = std::make_shared<BackedImage>(device, ve2, static_cast<VkFormat>(idata->format));
      break;
    default: CRASHRET(NULL); break;
    }
  }
  struct shaderGpuResources* gpuRes = resources.get(device);
  VkResult res;
  for(auto& pool : gpuRes->descPools) {
    gpuRes->descAllocInfo.descPool = pool;
    res = vkAllocateDescriptorSets(gpu->device, &gpuRes->descAllocInfo, &ret->descSet);
    if(!res) break;
  }
  if(res) {
    gpuRes->descAllocInfo.descPool = gpuRes->growDescPool(device);
    CRASHIFFAIL(vkAllocateDescriptorSets(gpu->device, &gpuRes->descAllocInfo, &ret->descSet));
  }
  ret->inited = false;
  ret->rpRes = std::make_unique<std::map<VkRenderPass, struct rpResources>>();
  return ret;
}

void Shader::makePipeForRP(VkRenderPass rp, GPU* gpu, VkPipeline* out) {//TODO call this
  struct shaderGpuResources* resources = resources.get(gpu);
  static VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, 0 };
  static VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, NULL, 0, 2, dynamicStateEnables };
  static VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0, 1, &viBinding, 2, viAttributes };
  static VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0,
						       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };
  static VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL, 0, VK_FALSE, VK_FALSE,
						       VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f };
  static VkPipelineColorBlendAttachmentState attState = { 0, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
							  VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xF };
  static VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL, 0,
						    0, VK_LOGIC_OP_NO_OP, 1, &attState, {1, 1, 1, 1} };
  static VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, 1, NULL, 1, NULL };
  static VkStencilOpState stencilOp = { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 };
  static VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL, 0,
						      1, 1, VK_COMPARE_OP_LESS_OR_EQUAL, 0, 0, stencilOp, stencilOp, 0, 0 };
  static VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL, 0,
						     VK_SAMPLE_COUNT_1_BIT, 0, 0, NULL, 0, 0 };
  static VkGraphicsPipelineCreateInfo pipeInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0, 2, NULL,
						   &vi, &ia, NULL, &vp, &rs, &ms, &ds, &cb, &dynamicState, &ret->pipelineLayout, NULL, 0, NULL, 0 };
  pipeInfo.stageCount = subshaders;
  pipeInfo.pStages = resources->stageInfos.get();
  pipeInfo.renderPass = rp;
  CRASHIFFAIL(vkCreateGraphicsPipelines(gpu->device, gpu->pipelineCache, 1, &pipeInfo, NULL, &pipeline));
  pipeInfo.pStages = pipeInfo.renderPass = pipeInfo.stageCount = 0;//don't leak handle to thread stack
}

void Shader::render(Queue::ExecutionPlan* ep, renderLayerMask layers, glm::dmat4 projection, GPU* gpu, VkRenderPass rp) {
  struct shaderGpuResources* resources = resources.get(gpu);
  auto rpr = resources->rpRes->[rp];//shared_ptr
  if(!rpr) {
    rpr = std::make_shared<struct rpResources>();
    makePipeForRP(rp, gpu, &rpr->pipeline);
    resources->rpRes->[rp] = rpr;
  }
  vkCmdBindPipeline(ep->getActive();, VK_PIPELINE_BIND_POINT_GRAPHICS, rpr->pipeline);
  for(renderLayerIdx rl = 0;rl < MAX_RENDER_LAYERS;rl++) {
    if(layers & (1 << rl)) {
      auto end = renderers[i].end();
      for(auto renderer = renderers[i].begin();renderer != end;renderer++)
	(*renderer)->render(projection, gpu);
    }
  }
}

void Shader::renderAll(Queue::ExecutionPlan* ep, renderLayerMask layers, glm::dmat4 projection, GPU* gpu, VkRenderPass rp) {
  auto end = allShaders.end();
  for(auto shader = allShaders.begin();shader != end;shader++)
    (*shader)->render(ep, layers, projection, gpu, rp);
}

Shader* WITE::Shader::make(const char** filepath, size_t files, struct shaderResourceLayoutEntry* srles, size_t resources) {
  return new Shader(filepath, files, slres, resources);
}

Shader* WITE::Shader::make(const char* filepathWildcard, struct shaderResourceLayoutEntry* srles, size_t resources) {
  return new Shader(filepathWildcard, slres, resources);
}

