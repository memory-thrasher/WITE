#include "ShaderModule.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  ShaderModule::ShaderModule(const void* code, size_t len,
			     std::initializer_list<std::pair<vk::ShaderStageFlagBits, const char*>> entryPoints) :
    handles(handles_t::creator_t_F::make(this, &ShaderModule::create),
	    handles_t::destroyer_t_F::make(this, &ShaderModule::destroy)),
    createInfo({}, len, reinterpret_cast<const uint32_t*>(code)),
    codeStart(code),
    codeLen(len)
  {
    for(auto pair : entryPoints)
      stageInfos[pair.first] = vk::PipelineShaderStageCreateInfo(psscf_none, pair.first, nullptr, pair.second, NULL);
  };

  vk::ShaderModule ShaderModule::create(size_t gpuIdx) {
    vk::ShaderModule ret;
    VK_ASSERT(Gpu::get(gpuIdx).getVkDevice().createShaderModule(&createInfo, ALLOCCB, &ret), "Failed to compile shader");
    return ret;
  };

  void ShaderModule::destroy(vk::ShaderModule& doomed, size_t gpuIdx) {
    Gpu::get(gpuIdx).getVkDevice().destroyShaderModule(doomed, ALLOCCB);
  };

  const vk::PipelineShaderStageCreateInfo&& ShaderModule::getCI(vk::ShaderStageFlagBits stage, size_t gpuIdx) {
    vk::PipelineShaderStageCreateInfo ret = stageInfos.at(stage);
    ret.module = handles.get(gpuIdx);
    return std::move(ret);
  };

}

