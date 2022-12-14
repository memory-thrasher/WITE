#pragma once

#include <map>

#include "GpuResource.hpp"
#include "StdExtensions.hpp"
#include "StructuralConstList.hpp"
#include "LiteralLookup.hpp"
#include "LiteralMap.hpp"

namespace WITE::GPU {

  defineLiteralMap(vk::QueueFlagBits, QueueType, queueTypeByFlagBit,
		   Collections::StructuralPair<vk::QueueFlagBits, QueueType>(vk::QueueFlagBits::eTransfer, QueueType::eTransfer),
		   Collections::StructuralPair<vk::QueueFlagBits, QueueType>(vk::QueueFlagBits::eCompute , QueueType::eCompute ),
		   Collections::StructuralPair<vk::QueueFlagBits, QueueType>(vk::QueueFlagBits::eGraphics, QueueType::eGraphics)
		   );

  defineLiteralMap(QueueType, vk::QueueFlagBits, queueBitByType,
		   Collections::StructuralPair<QueueType, vk::QueueFlagBits>(QueueType::eTransfer, vk::QueueFlagBits::eTransfer),
		   Collections::StructuralPair<QueueType, vk::QueueFlagBits>(QueueType::eCompute , vk::QueueFlagBits::eCompute ),
		   Collections::StructuralPair<QueueType, vk::QueueFlagBits>(QueueType::eGraphics, vk::QueueFlagBits::eGraphics)
		   );

  constexpr vk::ImageViewType getViewTypeForWhole(const ImageSlotData& isd) {
    size_t dimensions = isd.dimensions;
    bool isCube = isd.usage & GpuResource::USAGE_IS_CUBE;
    bool isArray = isd.arrayLayers > 1;
    // ASSERT_TRAP(!isCube || dimensions == 2, "Cube must be 2D (array optional)");
    // ASSERT_TRAP(dimensions <= 3, "Too many dimensions");
    // ASSERT_TRAP(!isArray || dimensions <= 2, "Too many dimensions for array");
    if(isCube) return isArray ? vk::ImageViewType::eCube : vk::ImageViewType::eCubeArray;
    switch(dimensions) {
    case 1: return isArray ? vk::ImageViewType::e1D : vk::ImageViewType::e1DArray;
    case 2: return isArray ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
    case 3: return vk::ImageViewType::e3D;
    default: return vk::ImageViewType::e1D;//invalid dimensionality
    // default: CRASHRET(vk::ImageViewType::e3D, "Illegal image");
    }
  };

  //NOTE USAGE_TRANSFER is not respected here because there is no one layout that always works with it
  defineLiteralLookup(usage_t, vk::ImageLayout, layoutUsageCompatibility,
		      Collections::StructuralPair<usage_t, std::initializer_list<vk::ImageLayout>>(GpuResource::USAGE_DS_SAMPLED, { vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eReadOnlyOptimal, vk::ImageLayout::eDepthStencilReadOnlyOptimal, vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal, vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal, vk::ImageLayout::eDepthReadOnlyOptimal, vk::ImageLayout::eStencilReadOnlyOptimal, vk::ImageLayout::eAttachmentFeedbackLoopOptimalEXT } ),
		      Collections::StructuralPair<usage_t, std::initializer_list<vk::ImageLayout>>(GpuResource::USAGE_ATT_INPUT, { vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eReadOnlyOptimal, vk::ImageLayout::eAttachmentOptimal, vk::ImageLayout::eDepthStencilReadOnlyOptimal, vk::ImageLayout::eDepthReadOnlyOptimal, vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal, vk::ImageLayout::eDepthReadOnlyOptimal, vk::ImageLayout::eStencilReadOnlyOptimal, vk::ImageLayout::eAttachmentFeedbackLoopOptimalEXT }),
		      Collections::StructuralPair<usage_t, std::initializer_list<vk::ImageLayout>>(GpuResource::USAGE_ATT_DEPTH, { vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageLayout::eDepthAttachmentOptimal, vk::ImageLayout::eStencilAttachmentOptimal, vk::ImageLayout::eAttachmentOptimal, vk::ImageLayout::eAttachmentFeedbackLoopOptimalEXT }),
		      Collections::StructuralPair<usage_t, std::initializer_list<vk::ImageLayout>>(GpuResource::USAGE_ATT_OUTPUT, { vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eAttachmentOptimal, vk::ImageLayout::eAttachmentFeedbackLoopOptimalEXT }),
		      Collections::StructuralPair<usage_t, std::initializer_list<vk::ImageLayout>>(GpuResource::USAGE_HOST_READ, { vk::ImageLayout::eTransferSrcOptimal }),
		      Collections::StructuralPair<usage_t, std::initializer_list<vk::ImageLayout>>(GpuResource::USAGE_HOST_WRITE, { vk::ImageLayout::eTransferDstOptimal }),
		      Collections::StructuralPair<usage_t, std::initializer_list<vk::ImageLayout>>(GpuResource::USAGE_PRESENT, { vk::ImageLayout::ePresentSrcKHR }));

  static constexpr vk::FormatFeatureFlags usageFormatFeatures(usage_t u) {
    vk::FormatFeatureFlags ret = (vk::FormatFeatureFlags)0;
    if(u & GpuResource::USAGE_VERTEX) ret |= vk::FormatFeatureFlagBits::eVertexBuffer;
    if(u & GpuResource::USAGE_DS_SAMPLED) {
      ret |= vk::FormatFeatureFlagBits::eSampledImage;
      if(u & GpuResource::USAGE_DS_WRITE) ret |= vk::FormatFeatureFlagBits::eStorageImage;
    }
    if(u & GpuResource::USAGE_ATT_DEPTH) ret |= vk::FormatFeatureFlagBits::eDepthStencilAttachment;
    if(u & GpuResource::USAGE_ATT_OUTPUT) ret |= vk::FormatFeatureFlagBits::eColorAttachment;
    if(u & GpuResource::USAGE_HOST_READ) ret |= vk::FormatFeatureFlagBits::eTransferSrc;
    if(u & GpuResource::USAGE_HOST_WRITE) ret |= vk::FormatFeatureFlagBits::eTransferDst;
    if(u & GpuResource::USAGE_TRANSFER) ret |= vk::FormatFeatureFlagBits::eTransferSrc | vk::FormatFeatureFlagBits::eTransferDst;
    return ret;
  };

  constexpr static vk::ImageLayout optimalLayoutForUsage(usage_t usage) {
    //TODO performance test what happens when we use general all the time (no transitions)
    if(!usage)
      return vk::ImageLayout::eGeneral;
    uint8_t firstUsageBit = 0;
    while((usage & (1 << firstUsageBit)) == 0) firstUsageBit++;
    for(uint8_t i = firstUsageBit;usage >> i;i++)
      if(usage & (1 << firstUsageBit) && !layoutUsageCompatibility.contains(1 << firstUsageBit))
	return vk::ImageLayout::eGeneral;
    const auto& options = layoutUsageCompatibility.at(firstUsageBit);
    for(vk::ImageLayout l : options) {
      bool valid = true;
      for(uint8_t i = firstUsageBit+1;usage >> i && valid;i++)
	if((usage & (1 << i)) && !layoutUsageCompatibility.at(i).contains(l))
	  valid = false;
      if(valid)
	return l;
    }
    return vk::ImageLayout::eGeneral;
  }

  constexpr static vk::AccessFlags2 accessFlagsFromUsage(usage_t usage) {
    vk::AccessFlags2 ret;
    if(usage & GpuResource::USAGE_INDIRECT) ret |= vk::AccessFlagBits2::eIndirectCommandRead;
    // if(usage & GpuResource::USAGE_NYI) ret |= vk::AccessFlagBits2::eIndexRead;
    if(usage & GpuResource::USAGE_VERTEX) ret |= vk::AccessFlagBits2::eVertexAttributeRead;
    if(usage & GpuResource::USAGE_DS_READ) ret |= vk::AccessFlagBits2::eUniformRead | vk::AccessFlagBits2::eMemoryRead;
    if(usage & GpuResource::USAGE_ATT_INPUT) ret |= vk::AccessFlagBits2::eInputAttachmentRead;
    if(usage & GpuResource::USAGE_DS_WRITE) ret |= vk::AccessFlagBits2::eMemoryWrite;
    //TODO? separate color attachment read/write (read is for blending)
    if(usage & GpuResource::USAGE_ATT_OUTPUT)
      ret |= vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead;
    if(usage & GpuResource::USAGE_ATT_DEPTH)
      ret |= vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    // if(usage & GpuResource::USAGE_HOST_READ) ret |= vk::AccessFlagBits2::eTransferRead;
    // if(usage & GpuResource::USAGE_HOST_WRITE) ret |= vk::AccessFlagBits2::eTransferWrite;
    if(usage & GpuResource::USAGE_HOST_READ) ret |= vk::AccessFlagBits2::eTransferRead;
    if(usage & GpuResource::USAGE_HOST_WRITE) ret |= vk::AccessFlagBits2::eTransferWrite;
    if(usage & GpuResource::USAGE_TRANSFER) ret |= vk::AccessFlagBits2::eTransferRead | vk::AccessFlagBits2::eTransferWrite;
    return ret;
  };

  constexpr static vk::PipelineStageFlags2 stagesFromAccess(vk::AccessFlags2 access) {
    if(access & (vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite))
      return vk::PipelineStageFlagBits2::eAllCommands;
    vk::PipelineStageFlags2 ret;
    if(access & vk::AccessFlagBits2::eIndirectCommandRead)
      ret |= vk::PipelineStageFlagBits2::eDrawIndirect | vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR;
    if(access & vk::AccessFlagBits2::eIndexRead)
      ret |= vk::PipelineStageFlagBits2::eVertexInput;
    if(access & vk::AccessFlagBits2::eVertexAttributeRead)
      ret |= vk::PipelineStageFlagBits2::eVertexInput;
    if(access & vk::AccessFlagBits2::eUniformRead)
      ret |= vk::PipelineStageFlagBits2::eTaskShaderNV | vk::PipelineStageFlagBits2::eMeshShaderNV |
	vk::PipelineStageFlagBits2::eRayTracingShaderNV | vk::PipelineStageFlagBits2::eVertexShader |
	vk::PipelineStageFlagBits2::eTessellationControlShader | vk::PipelineStageFlagBits2::eTessellationEvaluationShader |
	vk::PipelineStageFlagBits2::eGeometryShader | vk::PipelineStageFlagBits2::eFragmentShader |
	vk::PipelineStageFlagBits2::eComputeShader;
    if(access & vk::AccessFlagBits2::eShaderRead)
      ret |= vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR |// vk::PipelineStageFlagBits2::eMicromapBuildEXT |
	vk::PipelineStageFlagBits2::eTaskShaderNV | vk::PipelineStageFlagBits2::eMeshShaderNV |
	vk::PipelineStageFlagBits2::eRayTracingShaderNV | vk::PipelineStageFlagBits2::eVertexShader |
	vk::PipelineStageFlagBits2::eTessellationControlShader | vk::PipelineStageFlagBits2::eTessellationEvaluationShader |
	vk::PipelineStageFlagBits2::eGeometryShader | vk::PipelineStageFlagBits2::eFragmentShader |
	vk::PipelineStageFlagBits2::eComputeShader;
    if(access & vk::AccessFlagBits2::eShaderWrite)
      ret |= vk::PipelineStageFlagBits2::eTaskShaderNV | vk::PipelineStageFlagBits2::eMeshShaderNV |
	vk::PipelineStageFlagBits2::eRayTracingShaderNV | vk::PipelineStageFlagBits2::eVertexShader |
	vk::PipelineStageFlagBits2::eTessellationControlShader | vk::PipelineStageFlagBits2::eTessellationEvaluationShader |
	vk::PipelineStageFlagBits2::eGeometryShader | vk::PipelineStageFlagBits2::eFragmentShader |
	vk::PipelineStageFlagBits2::eComputeShader;
    if(access & vk::AccessFlagBits2::eInputAttachmentRead)
      ret |= vk::PipelineStageFlagBits2::eSubpassShadingHUAWEI | vk::PipelineStageFlagBits2::eFragmentShader;
    if(access & (vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite))
      ret |= vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    if(access & (vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite))
      ret |= vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
    if(access & (vk::AccessFlagBits2::eTransferRead | vk::AccessFlagBits2::eTransferWrite))
      ret |= vk::PipelineStageFlagBits2::eTransfer | vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR;
    if(access & (vk::AccessFlagBits2::eHostRead | vk::AccessFlagBits2::eHostWrite))
      ret |= vk::PipelineStageFlagBits2::eHost;
    if(access & vk::AccessFlagBits2::eColorAttachmentReadNoncoherentEXT)
      ret |= vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    if(access & (vk::AccessFlagBits2::eCommandPreprocessReadNV | vk::AccessFlagBits2::eCommandPreprocessWriteNV))
      ret |= vk::PipelineStageFlagBits2::eCommandPreprocessNV;
    if(access & vk::AccessFlagBits2::eConditionalRenderingReadEXT)
      ret |= vk::PipelineStageFlagBits2::eConditionalRenderingEXT;
    if(access & vk::AccessFlagBits2::eFragmentShadingRateAttachmentReadKHR)
      ret |= vk::PipelineStageFlagBits2::eFragmentShadingRateAttachmentKHR;
    if(access & vk::AccessFlagBits2::eInvocationMaskReadHUAWEI)
      ret |= vk::PipelineStageFlagBits2::eInvocationMaskHUAWEI;
    if(access & (vk::AccessFlagBits2::eTransformFeedbackWriteEXT | vk::AccessFlagBits2::eTransformFeedbackCounterWriteEXT))
      ret |= vk::PipelineStageFlagBits2::eTransformFeedbackEXT;
    if(access & vk::AccessFlagBits2::eTransformFeedbackCounterReadEXT)
      ret |= vk::PipelineStageFlagBits2::eTransformFeedbackEXT | vk::PipelineStageFlagBits2::eDrawIndirect;
    if(access & vk::AccessFlagBits2::eAccelerationStructureReadKHR)
      ret |= vk::PipelineStageFlagBits2::eTaskShaderNV | vk::PipelineStageFlagBits2::eMeshShaderNV |
	vk::PipelineStageFlagBits2::eRayTracingShaderNV | vk::PipelineStageFlagBits2::eVertexShader |
	vk::PipelineStageFlagBits2::eTessellationControlShader | vk::PipelineStageFlagBits2::eTessellationEvaluationShader |
	vk::PipelineStageFlagBits2::eGeometryShader | vk::PipelineStageFlagBits2::eFragmentShader |
	vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR;
    if(access & vk::AccessFlagBits2::eAccelerationStructureWriteKHR)
      ret |= vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR;
    if(access & vk::AccessFlagBits2::eFragmentDensityMapReadEXT)
      ret |= vk::PipelineStageFlagBits2::eFragmentDensityProcessEXT;
    return ret;
  };

  constexpr static vk::PipelineStageFlags2 stageFlagsForUsage(usage_t usage) {
    return stagesFromAccess(accessFlagsFromUsage(usage));
  };

  constexpr static vk::ImageAspectFlags aspectMaskForUsage(usage_t usage) {
    vk::ImageAspectFlags ret = vk::ImageAspectFlagBits::eMetadata;
    if(usage & GpuResource::USAGE_ATT_DEPTH)
      ret |= vk::ImageAspectFlagBits::eStencil | vk::ImageAspectFlagBits::eDepth;
    if(!ret || (usage & GpuResource::USAGE_ATT_OUTPUT))
      ret |= vk::ImageAspectFlagBits::eColor;
    //TODO other options if needed see vk::ImageAspectFlagBits
    return ret;
  };

}
