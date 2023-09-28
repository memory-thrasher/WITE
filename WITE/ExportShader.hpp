namespace WITE::Export {

  struct shader {

    struct module {
      using stage_t = vk::ShaderStageFlagBits;
      const char* code;
      stage_t stage;
      constexpr ShaderModule(const char* code, stage_t stage) : code(code), stage(stage) {};
    };

    enum class usage_t : uint8_t {
      indirectBuffer,
      indexBuffer,
      vertexBuffer,
      uniformBuffer,
      storageBuffer,
      pushConstant,
      sampledImage,
      storageImage,
      depthStencilAttachment, //maybe split into d/s/d+s ?
      inputAttachment,
      colorAttachment
    };

    struct subpassDatum {
      uint32_t resourceIdx;
      usageType_t type;
    };

    typedef LiteralList<subpassDatum> subpass_t;
    enum class type_t { graphics, compute };

    type_t type;
    LiteralList<ShaderModule> modules;
    subpass_t subpass;

  };

}
