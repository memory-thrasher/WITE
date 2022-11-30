#include "RenderTarget.hpp"

namespace WITE::GPU {

  template<StructuralConstList<RenderStep> STEPS, logicalDeviceMask_t LDM> class CubeMap : public BackedRenderTarget<{
    { GpuResourceType::eImage, {3, 32, 2, 1, 6, MIP_LEVELS, LDM,
	  GpuResource::USAGE_ATT_OUTPUT | GpuResource::USAGE_IS_CUBE | GpuResource::USAGE_DS_SAMPLED }},
    { GpuResourceType::eImage, {1, 32, 2, 1, 6, 1, LDM,
	  GpuResource::USAGE_ATT_DEPTH  | GpuResource::USAGE_IS_CUBE | GpuResource::USAGE_DS_SAMPLED }}
  }, STEPS, LDM> {
  public:
    static constexpr uint32_t MIP_LEVELS = 9;//2^MIP_LEVELS is the minimum resolution
    static constexpr ImageSlotData COLOR_SLOT = { 3, 32, 2, 1, 6, MIP_LEVELS, LDM,
      GpuResource::USAGE_ATT_OUTPUT | GpuResource::USAGE_IS_CUBE };
    static constexpr ImageSlotData DEPTH_SLOT = { 1, 32, 2, 1, 6, 1, LDM,
      GpuResource::USAGE_ATT_DEPTH | GpuResource::USAGE_IS_CUBE };
    Image<COLOR_SLOT> color;
    Image<DEPTH_SLOT> depth;
    CubeMap() : RenderTarget(LDM, layers, &color, &depth) {};
  };

};
