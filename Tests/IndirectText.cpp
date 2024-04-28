#include "../WITE/WITE.hpp"

//begin shared
constexpr uint64_t gpuId = 0;

//begin text
constexpr objectLayout OL_text = { .id = __LINE__ };

struct drawdata_t {//both drawdata in IndirectText.vert and color in soliddColor.frag
  glm::vec4 bbox, charMetric, color;
};

constexpr bufferRequirements BR_string = defineSimpleStorageBuffer(gpuId, 1024),
	    BR_S_string = NEW_ID(stagingRequirementsFor(BR_string, 2)),
	    BR_charData = defineSimpleStorageBuffer(gpuId, 128*4*2),//2x uint32 per possible char
	    BR_textIndirect = {
	      .deviceId = gpuId,
	      .id = __LINE__,
	      .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,//compute->vertex
	      .size = 4 + 1024*4*4,//uint32 char count + 1024 characters * 4 * uint32 each (indirect draw command)
	      .frameswapCount = 1,
	      .hostVisible = false,
	    }, BR_drawdata = defineSimpleUniformBuffer(gpuId, sizeof(drawdata_t));

constexpr resourceSlot RS_text_string = {
  .id = __LINE__,
  .requirementId = BR_string.id,
  .objectLayoutId = OL_text.id,
}, RS_text_S_string = {
  .id = __LINE__,
  .requirementId = BR_S_string.id,
  .objectLayoutId = OL_text.id,
}, RS_text_charData = {
  .id = __LINE__,
  .requirementId = BR_charData.id,
  .objectLayoutId = OL_text.id,
}, RS_text_indirect = {
  .id = __LINE__,
  .requirementId = BR_textIndirect.id,
  .objectLayoutId = OL_text.id,
  .external = true,
}, RS_text_drawdata = {
  .id = __LINE__,
  .requirementId = BR_drawdata.id,
  .objectLayoutId = OL_text.id,
};

//begin text/compute
#include "IndirectText.comp.spv.h"

constexpr resourceConsumer RC_S_textCompute_string = defineSBReadonlyConsumer(Compute),
	    RC_S_textCompute_charData = defineSBReadonlyConsumer(Compute),
	    RC_S_textCompute_indirectBuffer = defineSBWriteonlyConsumer(Compute),
	    RC_L_S_textCompute[] = { RC_S_textCompute_string, RC_S_textCompute_charData, RC_S_textCompute_indirectBuffer };

constexpr shaderModule SM_textCompute { IndirectText_comp, sizeof(IndirectText_comp), vk::ShaderStageFlagBits::eCompute };

constexpr computeShaderRequirements S_textCompute {
  .id = __LINE__,
  .module = &SM_textCompute,
  .sourceProvidedResources = RC_L_S_textCompute,
  .primaryOutputSlotId = RS_text_string.id,
  .strideX = 64,
};

//TODO more

//begin text/graphics
#include "IndirectText.vert.spv.h"
#include "solidColor.frag.spv.h"

//begin camera
constexpr uint64_t RC_ID_present_color = __LINE__;

//begin onion

int main(int argc, char** argv) {
  //
};

