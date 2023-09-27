#include "Vulkan.hpp"

namespace WITE::Export {

  struct ShaderModule {
    using stage_t = vk::ShaderStageFlagBits;
    const char* code;
    stage_t stage;
    constexpr ShaderModule(const char* code, stage_t stage) : code(code), stage(stage) {};
  };

}
