#pragma once

#include "wite_vulkan.hpp"
#include "literalList.hpp"
#include "udm.hpp"

namespace WITE {

  constexpr uint64_t NONE = std::numeric_limits<uint64_t>::max();
  constexpr size_t NONE_size = std::numeric_limits<size_t>::max();

  struct imageRequirements {
    uint64_t deviceId = NONE;
    uint64_t id = NONE;//unique among image, buffer, and subresource requirements
    vk::Format format = vk::Format::eUndefined;
    vk::ImageUsageFlags usage = {};//TODO calculator from AccessFlags
    uint8_t dimensions = 2, frameswapCount = 1;
    bool isCube = false, hostVisible = false;
    uint32_t arrayLayers = 1, mipLevels = 1;
    //MAYBE sample count
  };

  struct bufferRequirements {
    uint64_t deviceId = NONE;
    uint64_t id = NONE;//unique among image, buffer, and subresource requirements
    vk::BufferUsageFlags usage;
    uint32_t size = 0;
    uint8_t frameswapCount = 0;
    bool hostVisible = false;
    //MAYBE isTexel
  };

  enum class resourceUsageType { eOther, eDescriptor, eVertex };//other for copy and maybe others

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
    constexpr resourceUsage() : type(resourceUsageType::eOther) {};
    constexpr resourceUsage(const resourceUsage&) = default;
  };

  struct resourceConsumer {
    uint64_t id = NONE;//unique among copyStep.src/dst, resourceConsumer.id, clearStep.id, objectLayout.windowConsumerId, renderPassRequirements.depth/color
    //stage and access are not required for all commands, in some cases it's contextually clear
    vk::ShaderStageFlags stages;
    vk::AccessFlags2 access = {};
    resourceUsage usage;
  };

  struct unifiedSubresource {
    bool isDefault;
    vk::ImageSubresourceRange range;//image only
    vk::ImageViewType viewType = vk::ImageViewType::e2D;//image only
    vk::DeviceSize offset = 0, length = VK_WHOLE_SIZE;//buffer only
    constexpr unifiedSubresource() : isDefault(true) {};
    constexpr unifiedSubresource(vk::ImageSubresourceRange imageRange, vk::ImageViewType viewType = vk::ImageViewType::e2D) :
      isDefault(false), range(imageRange), viewType(viewType) {};
    constexpr unifiedSubresource(vk::DeviceSize offset, vk::DeviceSize length) :
      isDefault(false), offset(offset), length(length) {};
  };

  struct resourceReference {
    uint64_t resourceConsumerId = 0;//FK to exactly one of: copyStep.src/dst, resourceConsumer.id, clearStep.id
    uint64_t resourceSlotId = 0;
    uint8_t frameLatency = 0; //must be < requirement.frameswapCount. Generally 0 is the one being written this frame, 1 is the one that was written last frame.
    unifiedSubresource subresource = {};
  };

  enum class imageResizeType { eNone, eBlit, eClear, eDiscard };

  struct imageResizeBehavior {
    imageResizeType type;
    vk::ClearValue clearValue;
    bool trackWindow;
  };

  union resizeBehavior_t {
    imageResizeBehavior image;
    //MAYBE buffer version if needed
  };

  struct resourceSlot {
    uint64_t id = NONE;//unique among resource slots
    uint64_t requirementId;//FK to imageRequirement or bufferRequirement (never both!)
    uint64_t objectLayoutId;
    bool external = false;//for static or shared things like vertex buffers, must be assigned before render is called
    resizeBehavior_t resizeBehavior;
  };

  struct targetLayout {
    uint64_t id;//unique among source and target layouts
    uint64_t objectLayoutId;
    literalList<resourceReference> resources;
    bool selfRender = false;//controls whether sources belonging to the same object (not layout!) are rendered to this target
  };

  struct sourceLayout {
    uint64_t id;//unique among source and target layouts
    uint64_t objectLayoutId;
    literalList<resourceReference> resources;
  };

  struct objectLayout {
    uint64_t id;
    uint64_t windowConsumerId = NONE;//unique among copyStep.src/dst, resourceConsumer.id, clearStep.id, objectLayout.windowConsumerId, renderPassRequirements.depth/color. A resourceConsumer instance will be created with this id. Can only be consumed by references belonging to a targetLayout that is part of this objectLayout
  };

  struct shaderModule {
    const uint32_t* data;
    uint32_t size;
    vk::ShaderStageFlagBits stage;
    const char* entryPoint = "main";//NYI
    literalList<vk::SpecializationMapEntry> specializations;
    const void* const specializationData = NULL;
    size_t specializationDataSize = 0;
  };

#define defineShaderModules(NOM, ...) defineLiteralList(shaderModule, NOM, __VA_ARGS__)

  struct computeShaderRequirements {
    uint64_t id;//unique among shaders
    const shaderModule* module;//must be ptr or this struct becomes unusable in template args
    literalList<resourceConsumer> targetProvidedResources;
    literalList<resourceConsumer> sourceProvidedResources;
    uint64_t primaryOutputSlotId = NONE;
    //TODO more flexible stride. Need a square option: workgroupX = max(image.xy / strideX)
    //callback that takes resource size and returns workgroup size
    uint32_t strideX = 1, strideY = 1, strideZ = 1;//should probably be a multiple of local_size in shader
    //one invocation per member of primary output. If reference is an image, one invocation per stride pixels (workgroup [x, y, z] = pixels [x/strideX, y/strideY, z/strideZ; if image is 2d array, z = array layer. If reference is a buffer, one invocation per strideX of bytes (workgroup has only an X value, other strides ignored)
  };

  struct graphicsShaderRequirements {
    uint64_t id;//unique among shaders
    literalList<shaderModule> modules;
    literalList<resourceConsumer> targetProvidedResources;//idx of this within usage.type = binding id
    literalList<resourceConsumer> sourceProvidedResources;
    vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
    vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
    bool windCounterClockwise = false;
    vk::DeviceSize vertexCountOverride = 0, instanceCountOverride = 0;//0 means don't override, figure it out based on buffer size
    vk::PipelineColorBlendAttachmentState blend = { false, vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd, vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd, vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA };
  };

  struct renderPassRequirements {
    uint64_t id;//unique among render passes
    //unique among copyStep.src/dst, resourceConsumer.id, clearStep.id, objectLayout.windowConsumerId, renderPassRequirements.depth/color
    uint64_t depth, color; //MAYBE input attachment(s)  //ids for consumers that get generated with standard options
    bool clearDepth = false, clearColor = false;
    vk::ClearColorValue clearColorValue;
    vk::ClearDepthStencilValue clearDepthValue = { 1.0f, 0 };
    literalList<graphicsShaderRequirements> shaders;
  };

  struct copyStep {
    uint64_t id;
    uint64_t src, dst;//unique among copyStep.src/dst, resourceConsumer.id, clearStep.id, objectLayout.windowConsumerId, renderPassRequirements.depth/color
    vk::Filter filter = vk::Filter::eNearest;//only meaningful for image-to-image
  };

  struct clearStep {
    uint64_t id;//unique among copyStep.src/dst, resourceConsumer.id, clearStep.id, objectLayout.windowConsumerId, renderPassRequirements.depth/color
    vk::ClearValue clearValue;
  };

  struct layerRequirements {
    literalList<uint64_t> clears, copies, renders, computeShaders;
  };

  struct onionDescriptor {
    literalList<imageRequirements> IRS;
    literalList<bufferRequirements> BRS;
    literalList<resourceSlot> RSS;
    literalList<computeShaderRequirements> CSRS;
    literalList<renderPassRequirements> RPRS;
    literalList<clearStep> CLS;
    literalList<copyStep> CSS;
    literalList<layerRequirements> LRS;
    literalList<objectLayout> OLS;
    literalList<targetLayout> TLS;
    literalList<sourceLayout> SLS;
    uint64_t GPUID;
  };

}

