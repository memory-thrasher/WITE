#include "templateHelpers.hpp"
#include "onion.hpp"

namespace WITE {

  struct cubeFactoryData {
    uint64_t gpuId;
    uint64_t id_seed;
    resourceReference RR_rpColor, RR_rpDepth;
    literalList<uint64_t> RR_shaderTargetGlobalData, RR_shaderTargetPerSideData, RR_shaderSourceCubeSamplers;
    vk::Format format = Format::RGB32float;
    literalList<resourceMap> otherSourceResources;
    literalList<resourceMap> otherTargetResources;
  };

  template<cubeFactoryData CFD, typename TD = glm::vec4> struct cubeFactory {

    static constexpr uint64_t id_seed = CFD.id_seed, sourceIdSeed = id_seed + 100, targetIdSeed = id_seed + 200, targetPerSideIdSeed = id_seed + 300, targetPerSideIdStride = 100, gpuId = CFD.gpuId;

    static constexpr uint64_t sideId(uint32_t side, uint32_t idx) {
      return targetPerSideIdSeed + targetPerSideIdStride * side + idx;
    };

    struct targetViewportData_t {
      glm::mat4 transform;
    };

    struct targetData_t {
      targetViewportData_t perViewport[6];
      TD targetGlobals;
    };

    static constexpr bufferRequirements BR_targetData {
      .deviceId = gpuId,
      .id = id_seed,
      .usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .size = sizeof(targetData_t),
      .frameswapCount = 1,
      .hostVisible = false,
    };

    static constexpr bufferRequirements BRS_targetData {
      .deviceId = gpuId,
      .id = id_seed + 1,
      .usage = vk::BufferUsageFlagBits::eTransferSrc,
      .size = sizeof(targetData_t),
      .frameswapCount = 2,
      .hostVisible = true,
    };

    static constexpr imageRequirements IR_cubeColor {
      .deviceId = gpuId,
      .id = id_seed + 10,
      .format = CFD.format,
      .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
      .dimensions = 2,
      .frameswapCount = 2,
      .isCube = true,
      .hostVisible = false,
      .arrayLayers = 6,
    };

    static constexpr imageRequirements IR_depth {
      //not a cube because it doesn't need to be read as a cube, so just hand each side it's own image
      .deviceId = gpuId,
      .id = id_seed + 11,
      .format = Format::D16unorm,
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .dimensions = 2,
      .frameswapCount = 1,
      .hostVisible = false,
    };

    static constexpr copyStep C_updateTargetData {
      .id = targetIdSeed,
      .src = {
	.id = targetIdSeed + 1,
	.frameLatency = 1,
      },
      .dst = {
	.id = targetIdSeed + 2,
      },
    };

    static constexpr resourceMap RMS_cube {
      .id = sourceIdSeed + 1,
      .requirementId = IR_cubeColor.id,
      .resourceReferences = CFD.RR_shaderSourceCubeSamplers,
      .isCube = true,
      .resizeBehavior = { { imageResizeType::eBlit } },
    };

    static constexpr resourceMap RMS_targetData_staging {
      .id = sourceIdSeed + 2,
      .requirementId = BRS_targetData.id,
      .resourceReferences = C_updateTargetData.src.id,
    };

    static constexpr resourceMap RMS_targetData {
      .id = sourceIdSeed + 3,
      .requirementId = BR_targetData.id,
      .resourceReferences = C_updateTargetData.dst.id,
    };

    static constexpr resourceMap SL_auto[] = { RMS_targetData, RMS_targetData_staging, RMS_cube };

    static constexpr auto SL_resources = concat<resourceMap, SL_auto, CFD.otherSourceResources>();

    static constexpr sourceLayout SL = {
      .id = sourceIdSeed + 4,
      .resources = SL_resources,
    };

    static_assert((CFD.RR_rpColor.access & vk::AccessFlagBits2::eColorAttachmentWrite) &&
		  (CFD.RR_rpColor.stages & vk::ShaderStageFlagBits::eFragment) &&
		  (CFD.RR_rpDepth.access & vk::AccessFlagBits2::eDepthStencilAttachmentWrite) &&
		  (CFD.RR_rpDepth.stages & vk::ShaderStageFlagBits::eFragment));

    struct targetTemplates_t {
      resourceMap RMT_sideImage, RMT_sideData, RMT_globalData, RMT_sideDepth;
      copyableArray<resourceMap, 4> RMT_auto;
      copyableArray<resourceMap, 4+CFD.otherTargetResources.len> RMT_all;

      consteval targetTemplates_t(uint32_t i) :
	RMT_sideImage({
	  .id = sideId(i, 0),
	  .requirementId = IR_cubeColor.id,
	  .resourceReferences = CFD.RR_rpColor.id,
	  .external = true,
	  .subresource = { { vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, (uint32_t)i, 1 } },
	}),

	RMT_sideData({
	  .id = sideId(i, 1),
	  .requirementId = BR_targetData.id,
	  .resourceReferences = CFD.RR_shaderTargetPerSideData,
	  .external = true,
	  .subresource = { sizeof(targetViewportData_t)*i, sizeof(targetViewportData_t) },
	  }),

	RMT_globalData({
	  .id = sideId(i, 2),
	  .requirementId = BR_targetData.id,
	  .resourceReferences = CFD.RR_shaderTargetGlobalData,
	  .external = true,
	  .subresource = { sizeof(targetViewportData_t)*6, sizeof(TD) },
	}),

	RMT_sideDepth({
	  .id = sideId(i, 3),
	  .requirementId = IR_depth.id,
	  .resourceReferences = CFD.RR_rpDepth.id,
	}),

	RMT_auto({ RMT_sideImage, RMT_sideData, RMT_globalData, RMT_sideDepth }),
	RMT_all([this](size_t i) { return i < 4 ? RMT_auto[i] : CFD.otherTargetResources[i-4]; })//,
      {};

    };

    static constexpr copyableArray<targetTemplates_t, 6> targetTemplates = [](size_t i){ return i; };

    static constexpr copyableArray<targetLayout, 6> TL_sides = [](size_t i)consteval->targetLayout{
      return { .id = sideId(i, 4), .resources = targetTemplates[i].RMT_all };
    };

    template<uint64_t i, onionDescriptor OD>
    static auto createTarget(onion<OD>& o, auto src) {
      auto trg = o.template createTarget<TL_sides[i].id>();
      trg->template set<targetTemplates[i].RMT_sideImage.id>(&src->template get<RMS_cube.id>());
      trg->template set<targetTemplates[i].RMT_sideData.id>(&src->template get<RMS_targetData.id>());
      trg->template set<targetTemplates[i].RMT_globalData.id>(&src->template get<RMS_targetData.id>());
      return trg;
    };

    template<onionDescriptor OD> class cube_t {
    public:
      typedef onion<OD> onion_t;
      onion_t::template source_t<SL.id>* source;
      onion_t::template target_t<TL_sides[0].id>* target0;
      onion_t::template target_t<TL_sides[1].id>* target1;
      onion_t::template target_t<TL_sides[2].id>* target2;
      onion_t::template target_t<TL_sides[3].id>* target3;
      onion_t::template target_t<TL_sides[4].id>* target4;
      onion_t::template target_t<TL_sides[5].id>* target5;
      onion_t* o;

      void destroy() {
	if(!source) return;
	o->freeSource(source);
	source = NULL;
	o->freeTarget(target0);
	o->freeTarget(target1);
	o->freeTarget(target2);
	o->freeTarget(target3);
	o->freeTarget(target4);
	o->freeTarget(target5);
      };

      void updateTargetData(const glm::vec3& location, const TD& td) {
	targetData_t data {
	  {//note: glm consturctors are column-major-order
	    {{ 0, 0, 1, 0,  0,-1, 0, 0, -1, 0, 0, 0,  location.z,-location.y,-location.x, 1 }},//+x
	    {{ 0, 0, 1, 0,  0,-1, 0, 0,  1, 0, 0, 0,  location.z,-location.y, location.x, 1 }},//-x
	    {{ 1, 0, 0, 0,  0, 0, 1, 0,  0, 1, 0, 0,  location.x, location.z, location.y, 1 }},//+y
	    {{ 1, 0, 0, 0,  0, 0, 1, 0,  0,-1, 0, 0,  location.x, location.z,-location.y, 1 }},//-y
	    {{ 1, 0, 0, 0,  0,-1, 0, 0,  0, 0, 1, 0,  location.x,-location.y, location.z, 1 }},//+z
	    {{-1, 0, 0, 0,  0,-1, 0, 0,  0, 0, 1, 0, -location.x,-location.y, location.z, 1 }},//-z
	  },
	  td
	};
	source->template write<RMS_targetData_staging.id>(data);
      };

    };

    template<onionDescriptor OD> static void create(onion<OD>& o, cube_t<OD>* out) {
      out->o = &o;
      out->source = o.template createSource<SL.id>();
      out->target0 = createTarget<0>(o, out->source);
      out->target1 = createTarget<1>(o, out->source);
      out->target2 = createTarget<2>(o, out->source);
      out->target3 = createTarget<3>(o, out->source);
      out->target4 = createTarget<4>(o, out->source);
      out->target5 = createTarget<5>(o, out->source);
      //TODO tell those targets to ignore that specific source?
    };

  };

};

