#pragma once

#include <map>

#include "PerGpu.hpp"
#include "Vulkan.hpp"

namespace WITE::GPU {

  //intended for static storage
  class ShaderModule {
  private:
    typedef PerGpu<vk::ShaderModule> handles_t;
    handles_t handles;
    void create(vk::ShaderModule*, size_t gpu);
    void destroy(vk::ShaderModule*, size_t gpu);
    const vk::ShaderModuleCreateInfo createInfo;
    static constexpr vk::PipelineShaderStageCreateFlags psscf_none = (vk::PipelineShaderStageCreateFlags)0;
    std::map<vk::ShaderStageFlagBits, vk::PipelineShaderStageCreateInfo> stageInfos;
  public:
    const void* codeStart;
    const size_t codeLen;
    ShaderModule(const void* code, size_t len,
		 std::initializer_list<std::pair<vk::ShaderStageFlagBits, const char*>> entryPoints);
    template<size_t L> ShaderModule(const uint32_t (&a)[L], vk::ShaderStageFlagBits stage) :
      ShaderModule(reinterpret_cast<const void*>(a), L, {{ stage, "main" }}) {};
    const vk::PipelineShaderStageCreateInfo&& getCI(vk::ShaderStageFlagBits stage, size_t gpuIdx);
    bool has(vk::ShaderStageFlagBits b) const { return stageInfos.contains(b); };
  };

};
