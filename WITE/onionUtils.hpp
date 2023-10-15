namespace WITE {

  template<size_t FLOW_COUNT> struct packagedImageRequirements {

    CopyableArray<imageRequirements::flowStep, FLOW_COUNT> flow;
    imageRequirements requirements;

    template<typename T>
    constexpr packagedImageRequirements(T flows, const imageRequirements& ir) : flow(flows), requirements(ir) {
      requirements.flow = flow;
    };

    constexpr operator imageRequirements() const noexcept {
      return requirements;
    };

    //TODO finish when needed

  };

  //note: this does not create a flow
  consteval imageRequirements requirementsForSlot(const shaderImageSlot& sis) {
    imageRequirements ret;
    if(sis.writeStages) ret.usage |= vk::ImageUsageFlagBits::eStorage;
    if(sis.readStages) ret.usage |= vk::ImageUsageFlagBits::eSampled;
    ret.frameswapCount = sis.frameswapCount;
    ret.mipLevels = sis.mipLevels;
    ret.dimensions = sis.dimensions;
    ret.arrayLayers = sis.arrayLayers;
    ret.imageFlags |= sis.imageFlags;
    return ret;
  };

  consteval void assertValid(const imageRequirements& ir) {
    ASSERT_CONSTEXPR(ir.flow);
    ASSERT_CONSTEXPR(ir.dimensions > 1);
    ASSERT_CONSTEXPR(ir.dimensions < 4);
    ASSERT_CONSTEXPR(ir.arrayLayers > 0);
    ASSERT_CONSTEXPR(ir.dimensions < 3 || ir.arrayLayers == 1);
    ASSERT_CONSTEXPR(ir.frameswapCount > 0);
    ASSERT_CONSTEXPR(ir.mipLevels > 0);
    ASSERT_CONSTEXPR(ir.mipLevels == 1 || ir.dimensions > 1);
    ASSERT_CONSTEXPR((~ir.imageFlags & imageFlags_e::eCube) || (ir.arrayLayers % 6 == 0));
    ASSERT_CONSTEXPR((~ir.imageFlags & imageFlags_e::e3DIs2DArray) || (ir.dimensions == 3));
  };

  //imageRequirements is NOT a bitmask. & means union, as in "find criteria that will satisfy both these sets of criteria"
  consteval imageRequirements operator&(const imageRequirements& l, const imageRequirements& r) {
    if(l.frameswapCount == 0) return r;//l is not initialized, assume this is the first match in a compounding operation
    ASSERT_CONSTEXPR(l.format == r.format);
    ASSERT_CONSTEXPR(l.frameswapCount == r.frameswapCount);
    ASSERT_CONSTEXPR(l.mipLevels == r.mipLevels);
    ASSERT_CONSTEXPR(l.deviceId == r.deviceId);
    uint32_t arrayLayers;
    uint8_t dimensions;
    if(min(l.dimensions, r.dimensions) == 2 && max(l.dimensions, r.dimensions) == 3) {
      //special case: a 3D image can be used as an array of 2D images. In no case may a 3D image have >1 array layer.
      dimensions = 3;
      arrayLayers = 1;
    } else {
      ASSERT_CONSTEXPR(l.dimensions == r.dimensions);
      dimensions = l.dimensions;
      arrayLayers = max(l.arrayLayers, r.arrayLayers);
    }
    return {
      .deviceId = l.deviceId,
      .usage = r.usage | l.usage,
      .dimensions = dimensions,
      .framewswapCount = l.frameswapCount,
      .imageFlags = r.imageFlags | l.imageFlags,
      .arrayLayers = arrayLayers,
      .mipLevels = l.mipLevels
    };
  };

  consteval auto getImageFlow(uint64_t id, uint64_t onionId, const LiteralList<shader>& shaders) {
    std::vector<imageRequirements.flowStep> ret;
    for(uint64_t sid = 0;sid < shaders.len;sid++) {
      vk::ImageLayout layout = vk::ImageLayout::eUndefined;
      vk::PipelineStageFlagBits2 stages;
      vk::AccessFlags2 access;
      const shader& s = shaders[sid];
      //MAYBE assert no buffer use? Or implicitly create a texel buffer?
      for(const shaderImageSlot& sis : s.sampled) {
	if(sis.id == id) {
	  if(layout != vk::ImageLayout::eGeneral)
	    layout = sis.writeStages ? vk::ImageLayout::eGeneral : vk::ImageLayout::eReadOnlyOptimal;
	  stages |= sis.readStages | sis.writeStages;
	  access |= sis.writeStages ?
	    (vk::AccessFlagBits2::eShaderStorageWrite | (sis.readStages ? vk::AccessFlagBits2::eShaderStorageRead : 0)) :
	    vk::AccessFlagBits2::eShaderSampledRead;
	}
      }
      if(s.targetLink.depthStencil == id) {
	ASSERT_CONSTEXPR(layout == vk::ImageLayout::eUndefined);
	layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	stages |= vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
	access |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
      }
      if(s.targetLink.color == id) {
	ASSERT_CONSTEXPR(layout == vk::ImageLayout::eUndefined);
	layout = vk::ImageLayout::eColorAttachmentOptimal;
	stages |= vk::PipelineStageFlagBits2::eColorAttachmentOutput;
	access |= vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead;
      }
      if(layout == vk::ImageLayout::eUndefined) continue;
      if(ret.size() > 0 && ret[ret.size()-1].layout == layout) {
	ret[ret.size()-1].stages |= stages;
	continue;
      }
      ret.emplace_back({
	  .onionId = onionId,
	  .shaderId = sid,
	  .layout = layout,
	  .stages = stages,
	  .access = access
	});
    }
    return ret;
  };

  consteval auto getImageRequirements<uint64_t id, uint64_t onionId, const LiteralList<shader>& shaders, uint64_t gpuId>() {
    imageRequirements ret;
    bool initialized = false;
    for(const shader& s : shaders) {
      for(const shaderImageSlot& sis : s.sampled)
	if(sis.id == id)
	  ret &= requirementsForSlot(sis);
      if(s.targetLink.depthStencil == id) ret.usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
      if(s.targetLink.color == id) ret.usage |= vk::ImageUsageFlagBits::eColorAttachment;
    }
    return packedImageRequirements<getImageFlow(id, onionId, shaders)> { getImageFlow(id, ONION_ID, S), ret };
  };

  consteval auto getDefaultCI(imageRequirements r) {
    assertValid(r);
    vk::ImageCreateInfo ret {
      .imageType = (vk::ImageType)(r.dimensions-1),//yes, I'm that lazy
      .format = r.format,
      .mipLevels = r.mipLevels,
      .arrayLayers = r.arrayLayers,
      .samples = vk::SampleCountFlagBits::e1,
      //TODO complicate "hostVisible" with automatic staging zone
      .tiling = (r.imageFlags & imageFlags_e::hostVisible) ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal,
      .usage = r.usage,
      .sharingMode = vk::SharingMode::eExclusive;
    };
    if(r.imageFlags & imageFlags_e::eCube) ret.flags |= vk::ImageCreateFlagBits::eCubeCompatible;
    if(r.imageFlags & imageFlags_e::e3DIs2DArray) ret.flags |= vk::ImageCreateFlagBits::e2DArrayCompatible;
    return ret;
  };

}
