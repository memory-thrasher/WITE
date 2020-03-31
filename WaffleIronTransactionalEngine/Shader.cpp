#include "stdafx.h"
#include "Shader.h"

const struct extensionMeaning {
  const char* suffix;
  const VkShaderStageFlagBits stageBit;
} SUFFIXES[]{ { "vert.spv", VK_SHADER_STAGE_VERTEX_BIT },{ "frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT } };

Shader::Shader(const char** paths, size_t pathc, struct resourceLayoutEntry* res, size_t resc) :
  subshaderCount(pathc), resources(CallbackFactory<std::unique_ptr<shaderGpuResources>, GPU*>::
				   make<Shader>(this, Shader::makeDescriptors)), subshaders(new struct subshader_t[pathc]),
  resourcesPerInstance(resc), resourceLayout(new struct resourceLayoutEntry[resc]) {
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
  //TODO make pipeline
}

Shader::~Shader() {
}

void Shader::ensureResources(GPU* dev) {
  resources.get(dev);
}

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
  for (i = 0;i < resourcesPerInstance;i++) {
    ret->layoutBindings[i] = { i, (VkDescriptorType)resourceLayout[i].type, 1, resourceLayout[i].stage,
			       NULL };
  }
  ret->descLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0,
			  resourcesPerInstance, ret->layoutBindings.get() };
  CRASHIFFAIL(vkCreateDescriptorSetLayout(dev->device, &ret->descLayoutInfo, vkAlloc, &ret->descLayout));
  ret->pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0, 1,
			      &ret->descLayout, 0, NULL };
  CRASHIFFAIL(vkCreatePipelineLayout(dev->device, &ret->pipelineLayoutInfo, vkAlloc, &ret->pipelineLayout));
  return ret;
}

std::unique_ptr<Shader::Instance> Shader::makeResources(GPU* device) {
  std::unique_ptr<Instance> ret = std::make_unique<Instance>(resourcesPerInstance);
  for (size_t i = 0;i < resourcesPerInstance;i++) {
    if (!resourceLayout[i].perInstance) continue;
    switch (resourceLayout[i].type) {
    case SHADER_RESOURCE_UNIFORM:
      ret->resources.push_back(std::static_pointer_cast<ShaderResource>
			       (std::make_shared<BackedBuffer>(device,
								*static_cast<uint64_t*>(resourceLayout[i].moreData),
								VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)));
      break;
    case SHADER_RESOURCE_SAMPLED_IMAGE:
      struct imageData* idata = static_cast<struct imageData*>(resourceLayout[i].moreData);
      VkExtent2D ve2 = { idata->width, idata->height };
      ret->resources.push_back(std::static_pointer_cast<ShaderResource>
			       (std::make_shared<BackedImage>(device, ve2, static_cast<VkFormat>(idata->format))));
      break;
    default: CRASHRET(NULL);
    }
  }
  ret->inited = false;
  return ret;
}

void Shader::render(VkCommandBuffer cmd, renderLayerMask layers, glm::mat4d projection, GPU* gpu) {
  //TODO bind something for this shader?
  for(renderLayerIdx rl = 0;rl < MAX_RENDER_LAYERS;rl++) {
    if(layers & (1 << rl)) {
      auto end = renderers[i].end();
      for(auto renderer = renderers[i].begin();renderer != end;renderer++)
	(*renderer)->render(projection, gpu);
    }
  }
}

void Shader::renderAll(VkCommandBuffer cmd, renderLayerMask layers, glm::mat4d projection, GPU* gpu) {
  auto end = allShaders.end();
  for(auto shader = allShaders.begin();shader != end;shader++)
    (*shader)->render(cmd, layers, projection, gpu);
}

/*
  size_t i;
  struct resources_t* res;
  res = resources + dev->idx;
  if (res->stageInfos) return;
  res->moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, 0, NULL };
  res->stageInfos = new VkPipelineShaderStageCreateInfo[subshaderCount];
  for (i = 0;i < subshaderCount;i++) {
  res->stageInfos[i] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
  NULL, 0, subshaders[i].stageBit, NULL, "main", NULL };
  CRASHIFFAIL(vkCreateShaderModule(dev->device, &res->moduleInfo, vkAlloc, &res->stageInfos[i].module));
  }
  VkDescriptorSetLayoutBinding  layoutBindings[] = {
  { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL },
  { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL }//per texture?
  };//FIXME this needs an import pathway, that does not need VK includes. TODO derive size from this pathway?
  VkDescriptorSetLayoutCreateInfo descriptorLayout = {
  VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 2, layoutBindings };
  CRASHIFFAIL(vkCreateDescriptorSetLayout(dev->device, &descriptorLayout, vkAlloc, &res->descLayout));
  VkPipelineLayoutCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
  NULL, 0, 1, &res->descLayout, 0, NULL };
  CRASHIFFAIL(vkCreatePipelineLayout(dev->device, &pipelineInfo, vkAlloc, &res->pipelineLayout));
  }*/
