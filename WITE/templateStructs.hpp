#pragma once

#include "wite_vulkan.hpp"
#include "literalList.hpp"
#include "udm.hpp"

namespace WITE {

  constexpr uint64_t NONE = std::numeric_limits<uint64_t>::max();
  constexpr size_t NONE_size = std::numeric_limits<size_t>::max();

  struct imageRequirements {
    uint64_t deviceId = NONE;
    uint64_t id = NONE;//unique among image and buffer requirements
    vk::Format format = vk::Format::eUndefined;
    vk::ImageUsageFlags usage = {};//TODO calculator from AccessFlags
    uint8_t dimensions = 2, frameswapCount = 1;
    bool isCube = false, hostVisible = false;
    uint32_t arrayLayers = 1, mipLevels = 1;
    //MAYBE sample count
  };

  struct bufferRequirements {
    uint64_t deviceId = NONE;
    uint64_t id = NONE;//unique among image and buffer requirements
    vk::BufferUsageFlags usage;
    uint32_t size = 0;
    uint8_t frameswapCount = 0;
    bool hostVisible = false;
    //MAYBE isTexel
  };

  enum class imageResizeType { eNone, eBlit, eClear, eDiscard };

  struct imageResizeBehavior {
    imageResizeType type;
    vk::ClearValue clearValue;
    bool trackWindow;
  };

  union resizeBehavior_t {
    imageResizeBehavior image;
    //just in case we want to do that with buffers too someday
  };

  enum class resourceUsageType { eNone, eDescriptor, eVertex };//none for copy and maybe others

  struct resourceUsage {
    resourceUsageType type;
    union {
      struct {
	vk::DescriptorType descriptorType;
	vk::SamplerCreateInfo sampler;//only for when decriptorType is sampler or combinedSampler
      } asDescriptor;
      struct {
	udm format;
	vk::VertexInputRate rate;
      } asVertex;
    };
    constexpr resourceUsage(vk::DescriptorType dt, vk::SamplerCreateInfo sci = {}) : type(resourceUsageType::eDescriptor), asDescriptor({ dt, sci }) {};
    constexpr resourceUsage(udm format, vk::VertexInputRate vir) : type(resourceUsageType::eVertex), asVertex({ format, vir }) {};
    constexpr resourceUsage() : type(resourceUsageType::eNone) {};
    constexpr resourceUsage(const resourceUsage&) = default;
    // constexpr std::strong_ordering operator<=>(const resourceUsage& r) const {
    //   std::strong_ordering comp = type <=> r.type;
    //   if(comp != 0) return comp;
    //   switch(type) {
    //   case resourceusageType::eNone: return std::strong_ordering::equal;
    //   case resourceusageType::eDescriptor: return descriptorType <=> r.descriptorType;
    //   }
    // };
  };

  struct resourceReference {
    uint64_t id;//unique among resource references
    //stage and access are not required for all commands, in some cases it's contextually clear
    vk::ShaderStageFlags stages;
    vk::AccessFlags2 access = {};
    uint8_t frameLatency = 0; //must be < requirement.frameswapCount. Generally 0 is the one being written this frame, 1 is the one that was written last frame.
    resourceUsage usage;
  };

  struct resourceMap {
    uint64_t id = NONE;//unique among resource maps
    uint64_t requirementId;//FK to imageRequirement or bufferRequirement (never both!)
    literalList<uint64_t> resourceReferences;//FK to resourceReference
    uint8_t hostAccessOffset = 0;
    bool external = false;//for static or shared things like vertex buffers, must be assigned before render is called
    resizeBehavior_t resizeBehavior;
  };

  struct targetLayout {
    uint64_t id;//unique among source and target layouts
    literalList<resourceMap> resources;
    uint64_t presentImageResourceMapId = NONE;
    uint8_t presentFrameLatency = 0;
  };

  struct sourceLayout {
    uint64_t id;//unique among source and target layouts
    literalList<resourceMap> resources;
  };

  struct shaderModule {
    const uint32_t* data;
    uint32_t size;
    vk::ShaderStageFlagBits stage;
    const char* entryPoint = "main";//NYI
  };

#define defineShaderModules(NOM, ...) defineLiteralList(shaderModule, NOM, __VA_ARGS__)

  struct computeShaderRequirements {
    uint64_t id;//unique among shaders
    const shaderModule* module;//must be ptr or this struct becomes unusable in template args
    literalList<resourceReference> targetProvidedResources;
    literalList<resourceReference> sourceProvidedResources;
    uint64_t primaryOutputReferenceId = NONE;
    //TODO more flexible stride. Need a square option: workgroupX = max(image.xy / strideX)
    //callback that takes resource size and returns workgroup size
    uint32_t strideX = 1, strideY = 1, strideZ = 1;//should probably be a multiple of local_size in shader
    //one invocation per member of primary output. If reference is an image, one invocation per stride pixels (workgroup [x, y, z] = pixels [x/strideX, y/strideY, z/strideZ; if image is 2d array, z = array layer. If reference is a buffer, one invocation per strideX of bytes (workgroup has only an X value, other strides ignored)
  };

  struct graphicsShaderRequirements {
    uint64_t id;//unique among shaders
    literalList<shaderModule> modules;
    literalList<resourceReference> targetProvidedResources;//idx of this within usage.type = binding id
    literalList<resourceReference> sourceProvidedResources;
    vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
    vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
    bool windCounterClockwise = false;
    vk::DeviceSize vertexCountOverride = 0, instanceCountOverride = 0;//0 means don't override, figure it out based on buffer size
  };

  struct renderPassRequirements {
    uint64_t id;//unique among render passes
    resourceReference depth, color; //MAYBE input attachment
    bool clearDepth = false, clearColor = false;
    vk::ClearColorValue clearColorValue;
    vk::ClearDepthStencilValue clearDepthValue = { 1.0f, 0 };
    literalList<graphicsShaderRequirements> shaders;
  };

  struct copyStep {
    uint64_t id;
    resourceReference src, dst;
    vk::Filter filter = vk::Filter::eNearest;//only meaningful for image-to-image
  };

  struct clearStep {
    uint64_t id;
    resourceReference rr;
    vk::ClearValue clearValue;
  };

  //NOTE: barriers are NOT allowed between executions of individual copies or shaders of the same type. Barriers are allowed between types of executions. Barrier between copy and render is allowed, barrier between shader 1 and 2 of the same RP is not.
  struct layerRequirements {
    literalList<uint64_t> sourceLayouts, targetLayouts, clears, copies, renders, computeShaders;
  };

  struct onionDescriptor {
    literalList<imageRequirements> IRS;
    literalList<bufferRequirements> BRS;
    literalList<computeShaderRequirements> CSRS;
    literalList<renderPassRequirements> RPRS;
    literalList<clearStep> CLS;
    literalList<copyStep> CSS;
    literalList<layerRequirements> LRS;
    literalList<targetLayout> TLS;
    literalList<sourceLayout> SLS;
    uint64_t GPUID;
  };

  /*
note: this is missing some stuff but you get the idea:
onion
  >= 0 image requirements
  >= 0 buffer requirements
  compute shader
    >=0 source resource references
    >=0 target resource references
    (no linkage, cannot attach)
    1 command buffer
  >= 0 render pass
    1 shader target linkage
      target resource reference for each of
        1 depthStencil
        1 color
    >= 1 shader
      >= 1 shader module
      >= 0 target resource references
        for resources not in linkage (like projection transforms)
      >= 0 source resource references
    1 command buffer
  >= 0 copy command
    src resource reference
    dst resource reference
      must be same layout owner (source or target) as src id
    1 of: image/image, image/buffer, buffer/image, buffer/buffer
      dynamic by resource reference?
    subrange?
    1 command buffer
  >= 1 layer
    >= 0 source layout ids
    >= 0 target layout ids
    all steps executed in the order they appear
    depending on which layouts are provided, one of:
      perSource
        >= 0 copy command ids
          cannot have per-target resources
        >= 0 compute shader ids
          cannot have per-target resources
      perSourcePerTarget
        >= 0 copy command ids
        >= 0 render pass ids
        >= 0 compute shader ids
      perTarget
        >= 0 copy command ids
          cannot have per-source resources
        >= 0 render pass ids
          requires target resource
          cannot have per-source resources
          example: hud overlay
        >= 0 compute shader ids
          cannot have per-source resources
  >= 0 target layouts
    >= 1 resource map
      >= 1 resource reference id
  >= 0 source layouts
    >= 1 resource map
      see target layout

   */

}

