#pragma once

#include <algorithm>
#include <compare>

#include "StructuralConstList.hpp"
#include "LiteralMap.hpp"
#include "StdExtensions.hpp"
#include "Image.hpp"
#include "StructuralMap.hpp"
#include "LiteralJaggedList.hpp"

namespace WITE::GPU {

  // enum class ShaderResourceType { eBuffer, eImage };
  enum class ShaderStage { eDraw, eAssembler, eVertex, eTessellation, eGeometry, eFragmentDepth, eFragment, eBlending,
    eTask, eMesh, eCompute, last = eCompute };

  enum class ShaderResourceAccessType { eUndefined, eVertex, eUniform, eUniformTexel, eSampled, eDepthAtt, eColorAtt,
    last = eColorAtt };

  enum class ShaderResourceProvider { eRenderable, eRenderTarget, eShaderInstance, last = eShaderInstance };

  defineLiteralMap(ShaderStage, vk::ShaderStageFlags, shaderStageMap,
		   Collections::StructuralPair<ShaderStage, vk::ShaderStageFlags>(ShaderStage::eVertex, vk::ShaderStageFlagBits::eVertex),
		   Collections::StructuralPair<ShaderStage, vk::ShaderStageFlags>(ShaderStage::eTessellation, vk::ShaderStageFlagBits::eTessellationControl | vk::ShaderStageFlagBits::eTessellationEvaluation),
		   Collections::StructuralPair<ShaderStage, vk::ShaderStageFlags>(ShaderStage::eGeometry, vk::ShaderStageFlagBits::eGeometry),
		   Collections::StructuralPair<ShaderStage, vk::ShaderStageFlags>(ShaderStage::eFragment, vk::ShaderStageFlagBits::eFragment),
		   Collections::StructuralPair<ShaderStage, vk::ShaderStageFlags>(ShaderStage::eFragmentDepth, vk::ShaderStageFlagBits::eFragment),
		   Collections::StructuralPair<ShaderStage, vk::ShaderStageFlags>(ShaderStage::eTask, vk::ShaderStageFlagBits::eTaskNV),
		   Collections::StructuralPair<ShaderStage, vk::ShaderStageFlags>(ShaderStage::eMesh, vk::ShaderStageFlagBits::eMeshNV),
		   Collections::StructuralPair<ShaderStage, vk::ShaderStageFlags>(ShaderStage::eCompute, vk::ShaderStageFlagBits::eCompute));

  //TODO look into VkDescriptorSetVariableDescriptorCountAllocateInfo

  constexpr uint64_t hashCode(bool write, ShaderStage stage, ShaderResourceAccessType access) {
    return (static_cast<uint64_t>(stage) * (static_cast<uint64_t>(ShaderStage::last) + 1) +
	    static_cast<uint64_t>(access)) * (static_cast<uint64_t>(ShaderResourceAccessType::last) + 1) * 2 + write;
  };

  class ShaderResourceUsage {
  public:
    const bool write;
    const ShaderStage stage;
    const ShaderResourceAccessType access;

    ShaderResourceUsage() = delete;
    constexpr ShaderResourceUsage(const bool write, const ShaderStage stage,
				  const ShaderResourceAccessType access = ShaderResourceAccessType::eUndefined) :
      write(write), stage(stage), access(access) {};
    constexpr ~ShaderResourceUsage() = default;

    constexpr bool operator==(const ShaderResourceUsage& a) const {
      return a.write == this->write && a.stage == this->stage && a.access == this->access;
    };

    //guaranteed unique
    constexpr uint64_t hashCode() const {
      return GPU::hashCode(write, stage, access);
    };

    constexpr auto operator <=>(const ShaderResourceUsage& u) {
      return hashCode() <=> u.hashCode();
    };

    static constexpr uint64_t maxHashCode = GPU::hashCode(true, ShaderStage::last, ShaderResourceAccessType::last)+1;

    constexpr bool isDS() const {
      return access == ShaderResourceAccessType::eUniform ||
	access == ShaderResourceAccessType::eUniformTexel ||
	access == ShaderResourceAccessType::eSampled;
    };

    constexpr vk::DescriptorType getVkDSType() const {
      switch(access) {
      case ShaderResourceAccessType::eUniform: return write ? vk::DescriptorType::eStorageBuffer : vk::DescriptorType::eUniformBuffer;
      case ShaderResourceAccessType::eUniformTexel: return write ? vk::DescriptorType::eStorageTexelBuffer : vk::DescriptorType::eUniformTexelBuffer;
      case ShaderResourceAccessType::eSampled: return write ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eSampledImage;
      default: throw "illegal access"; break;
      };
    };

    constexpr vk::ShaderStageFlags getVkStages() const {
      return shaderStageMap[stage];
    };
  };

  /*
    multiple usages per resources should be rare. Each usage where isDS()=true gets it's own descriptor slot!
   */
  class ShaderResource {
  public:
    // const ShaderResourceType type;
    const Collections::StructuralConstList<ShaderResourceUsage> usage;
    const ShaderResourceProvider providedBy;
    ShaderResource() = delete;
    constexpr ShaderResource(const ShaderResource&) = default;
    template<class Iter> constexpr ShaderResource(const ShaderResourceProvider provider, Iter B, Iter E) :
      usage(B, E), providedBy(provider) {};
    template<class Container> constexpr ShaderResource(const ShaderResourceProvider provider, Container C) :
      ShaderResource(provider, C.begin(), C.end()) {};
    constexpr ShaderResource(// const ShaderResourceType type, 
			     const std::initializer_list<ShaderResourceUsage> usage,
			     const ShaderResourceProvider provider) :
      // type(type),
      usage(usage),
      providedBy(provider)
    {};

    template<class Resource> struct acceptsTest { constexpr bool operator()(const ShaderResource*) { return false; }; };

    template<ImageSlotData ISD> struct acceptsTest<Image<ISD>> {
      //TODO convert from lamda to foreach
      constexpr bool operator()(const ShaderResource* s) {
	for(auto u : s->usage) {
	  switch(u.access) {
	  case ShaderResourceAccessType::eUndefined:
	    switch(u.stage) {
	    case ShaderStage::eDraw: if(!(ISD.usage & ImageBase::USAGE_INDIRECT)) return false; break;
	    case ShaderStage::eAssembler: break;
	    default: return false; break;
	    }
	    break;
	  case ShaderResourceAccessType::eVertex: if(!(ISD.usage & ImageBase::USAGE_VERTEX)) return false; break;
	  case ShaderResourceAccessType::eUniform:
	  case ShaderResourceAccessType::eUniformTexel:
	  case ShaderResourceAccessType::eSampled:
	    if(!u.write && !(ISD.usage & (ImageBase::USAGE_DS_READ | ImageBase::USAGE_DS_SAMPLED |
					  ImageBase::USAGE_ATT_INPUT))) return false;
	    if(u.write && !(ISD.usage & ImageBase::USAGE_DS_WRITE)) return false;
	    break;
	  case ShaderResourceAccessType::eDepthAtt: if(!(ISD.usage & ImageBase::USAGE_ATT_DEPTH)) return false; break;
	  case ShaderResourceAccessType::eColorAtt: if(!(ISD.usage & ImageBase::USAGE_ATT_OUTPUT)) return false; break;
	  }
	}
	return true;
      };
    };

    //TODO ^for buffers?
    template<class Resource> constexpr bool accepts() { return acceptsTest<Resource>::accepts(this); };

    //guaranteed unique
    constexpr uint64_t hashCode() const {
      uint64_t ret = static_cast<uint64_t>(providedBy);
      for(ShaderResourceUsage u : usage)
	ret = ret * ShaderResourceUsage::maxHashCode + u.hashCode();
      return ret;
    };

    constexpr bool contains(const ShaderResourceAccessType access, const bool write) const {
      for(auto c : usage)
	if(c.write == write && c.access == access)
	  return true;
      return false;
    };
  };

  struct ShaderData {
  public:
    static constexpr ShaderResourceUsage Indirect = { false, ShaderStage::eDraw },
      Index = { false, ShaderStage::eAssembler },
      Vertex = { false, ShaderStage::eAssembler, ShaderResourceAccessType::eVertex },
    //Vertex
      UniformToVertex = { false, ShaderStage::eVertex, ShaderResourceAccessType::eUniform },
      UniformTexelToVertex = { false, ShaderStage::eVertex, ShaderResourceAccessType::eUniformTexel },
      ImageToVertex = { false, ShaderStage::eVertex, ShaderResourceAccessType::eSampled },
      StorageUniformToVertex = { true, ShaderStage::eVertex, ShaderResourceAccessType::eUniform },
      StorageUniformTexelToVertex = { true, ShaderStage::eVertex, ShaderResourceAccessType::eUniformTexel },
      StorageImageToVertex = { true, ShaderStage::eVertex, ShaderResourceAccessType::eSampled },
    //Tessellation
      UniformToTessellation = { false, ShaderStage::eTessellation, ShaderResourceAccessType::eUniform },
      UniformTexelToTessellation = { false, ShaderStage::eTessellation, ShaderResourceAccessType::eUniformTexel },
      ImageToTessellation = { false, ShaderStage::eTessellation, ShaderResourceAccessType::eSampled },
      StorageUniformToTessellation = { true, ShaderStage::eTessellation, ShaderResourceAccessType::eUniform },
      StorageUniformTexelToTessellation = { true, ShaderStage::eTessellation, ShaderResourceAccessType::eUniformTexel },
      StorageImageToTessellation = { true, ShaderStage::eTessellation, ShaderResourceAccessType::eSampled },
    //Geometry
      UniformToGeometry = { false, ShaderStage::eGeometry, ShaderResourceAccessType::eUniform },
      UniformTexelToGeometry = { false, ShaderStage::eGeometry, ShaderResourceAccessType::eUniformTexel },
      ImageToGeometry = { false, ShaderStage::eGeometry, ShaderResourceAccessType::eSampled },
      StorageUniformToGeometry = { true, ShaderStage::eGeometry, ShaderResourceAccessType::eUniform },
      StorageUniformTexelToGeometry = { true, ShaderStage::eGeometry, ShaderResourceAccessType::eUniformTexel },
      StorageImageToGeometry = { true, ShaderStage::eGeometry, ShaderResourceAccessType::eSampled },
    //Fragment
      UniformToFragment = { false, ShaderStage::eFragment, ShaderResourceAccessType::eUniform },
      UniformTexelToFragment = { false, ShaderStage::eFragment, ShaderResourceAccessType::eUniformTexel },
      ImageToFragment = { false, ShaderStage::eFragment, ShaderResourceAccessType::eSampled },
      StorageUniformToFragment = { true, ShaderStage::eFragment, ShaderResourceAccessType::eUniform },
      StorageUniformTexelToFragment = { true, ShaderStage::eFragment, ShaderResourceAccessType::eUniformTexel },
      StorageImageToFragment = { true, ShaderStage::eFragment, ShaderResourceAccessType::eSampled },
      AttachmentToFragment = { false, ShaderStage::eFragment, ShaderResourceAccessType::eColorAtt },
    //DepthFragment
      DepthAttachmentRO = { false, ShaderStage::eFragmentDepth, ShaderResourceAccessType::eDepthAtt },
      DepthAttachment = { true, ShaderStage::eFragmentDepth, ShaderResourceAccessType::eDepthAtt },
    //Blending
      ColorAttachmentRO = { false, ShaderStage::eBlending, ShaderResourceAccessType::eColorAtt },
      ColorAttachment = { true, ShaderStage::eBlending, ShaderResourceAccessType::eColorAtt },
    //Task
      UniformToTask = { false, ShaderStage::eTask, ShaderResourceAccessType::eUniform },
      UniformTexelToTask = { false, ShaderStage::eTask, ShaderResourceAccessType::eUniformTexel },
      ImageToTask = { false, ShaderStage::eTask, ShaderResourceAccessType::eSampled },
      StorageUniformToTask = { true, ShaderStage::eTask, ShaderResourceAccessType::eUniform },
      StorageUniformTexelToTask = { true, ShaderStage::eTask, ShaderResourceAccessType::eUniformTexel },
      StorageImageToTask = { true, ShaderStage::eTask, ShaderResourceAccessType::eSampled },
    //Mesh
      UniformToMesh = { false, ShaderStage::eMesh, ShaderResourceAccessType::eUniform },
      UniformTexelToMesh = { false, ShaderStage::eMesh, ShaderResourceAccessType::eUniformTexel },
      ImageToMesh = { false, ShaderStage::eMesh, ShaderResourceAccessType::eSampled },
      StorageUniformToMesh = { true, ShaderStage::eMesh, ShaderResourceAccessType::eUniform },
      StorageUniformTexelToMesh = { true, ShaderStage::eMesh, ShaderResourceAccessType::eUniformTexel },
      StorageImageToMesh = { true, ShaderStage::eMesh, ShaderResourceAccessType::eSampled },
    //Compute
      UniformToCompute = { false, ShaderStage::eCompute, ShaderResourceAccessType::eUniform },
      UniformTexelToCompute = { false, ShaderStage::eCompute, ShaderResourceAccessType::eUniformTexel },
      ImageToCompute = { false, ShaderStage::eCompute, ShaderResourceAccessType::eSampled },
      StorageUniformToCompute = { true, ShaderStage::eCompute, ShaderResourceAccessType::eUniform },
      StorageUniformTexelToCompute = { true, ShaderStage::eCompute, ShaderResourceAccessType::eUniformTexel },
      StorageImageToCompute = { true, ShaderStage::eCompute, ShaderResourceAccessType::eSampled };

    const Collections::StructuralConstList<ShaderResource> resources;
    constexpr ~ShaderData() = default;
    constexpr ShaderData(const std::initializer_list<ShaderResource> resources) : resources(resources) {};
    template<class Iter> constexpr ShaderData(const Iter B, const Iter E) : resources(B, E) {};
    template<class Container> constexpr ShaderData(const Container C) : ShaderData(C.begin(), C.end()) {};
    const ShaderResource& operator[](const size_t i) const {
      return resources[i];
    };

    constexpr ShaderData SubsetFrom(ShaderResourceProvider p) const {
      std::vector<ShaderResource> retResources;
      for(auto r : resources)
	if(r.providedBy == p)
	  retResources.push_back(r);
      return ShaderData(retResources);
    };

    typedef uint64_t hashcode_t;//used in maps to cache descriptor info
    //uniqueness likely but not gauranteed
    constexpr hashcode_t hashCode() const {
      uint64_t ret;
      for(ShaderResource r : resources)
	ret = ret * 5021 + r.hashCode();
      return ret;
    };

    constexpr inline operator hashcode_t() { return hashCode(); };

    constexpr bool contains(const ShaderResourceUsage u) const {
      for(auto r : resources)
	for(auto cu : r.usage)
	  if(cu == u)
	    return true;
      return false;
    };

    constexpr bool contains(const ShaderStage stage) const {
      for(auto r : resources)
	for(auto cu : r.usage)
	  if(cu.stage == stage)
	    return true;
      return false;
    };

    constexpr bool contains(const ShaderResourceAccessType access) const {
      for(ShaderResource r : resources)
	for(ShaderResourceUsage cu : r.usage)
	  if(cu.access == access)
	    return true;
      return false;
    };

    constexpr bool containsWritable() const {
      for(auto r : resources)
	for(auto cu : r.usage)
	  if(cu.write)
	    return true;
      return false;
    };

    constexpr bool containsOnly(const std::initializer_list<ShaderStage> stages) const {
      for(auto r : resources)
	for(auto cu : r.usage)
	  if(Collections::contains(stages, cu.stage))
	    return true;
      return false;
    };

    constexpr size_t countDescriptorSetResources() const {
      size_t ret = 0;
      std::vector<vk::DescriptorType> types;
      for(ShaderResource r : resources) {
	types.clear();
	for(ShaderResourceUsage cu : r.usage) {
	  auto dst = cu.getVkDSType();
	  if(cu.isDS() && !Collections::contains(types, dst)) {
	    ret++;
	    types.push_back(dst);
	  }
	}
      }
      return ret;
    };

    template<size_t retCount> //must always = countDescriptorSetResources()
    constexpr auto getDescriptorSetLayoutBindings() const {
      Collections::StructuralMap<vk::DescriptorType, vk::ShaderStageFlags> flags;
      vk::DescriptorSetLayoutBinding ret[retCount];
      uint32_t i = 0;
      for(ShaderResource r : resources) {
	flags.clear();
	for(ShaderResourceUsage cu : r.usage)
	  if(cu.isDS())
	    flags[cu.getVkDSType()] |= cu.getVkStages();
	for(auto p : flags)
	  ret[i] = { i++, p.k, 1, p.v };
      }
      return ret;
    };

    template<size_t bindingCount> //must always = countDescriptorSetResources()
    constexpr void poolSizesHelper(Collections::StructuralMap<vk::DescriptorType, uint32_t>& ret) const {
      auto bindings = getDescriptorSetLayoutBindings<bindingCount>();
      for(auto binding : bindings)
	ret[binding.descriptorType]++;
    }

    template<size_t bindingCount> //must always = countDescriptorSetResources()
    constexpr uint32_t countDistinctTypes() const {
      Collections::StructuralMap<vk::DescriptorType, uint32_t> helper;
      poolSizesHelper<bindingCount>(helper);
      return helper.size();
    };

    template<size_t retCount, size_t bindingCount> //must always = countDistinctTypes(), countDescriptorSetResources()
    constexpr auto getDescriptorPoolSizes() const {
      vk::DescriptorPoolSize ret[retCount];
      size_t i = 0;
      Collections::StructuralMap<vk::DescriptorType, uint32_t> sizes;
      poolSizesHelper<bindingCount>(sizes);
      for(auto pair : sizes)
	ret[i++] = { pair.k, pair.v };
      return ret;
    };

    constexpr size_t count(const ShaderResourceUsage& usage) const {
      size_t ret = 0;
      for(ShaderResource r : resources)
	for(ShaderResourceUsage cu : r.usage)
	  if(cu == usage)
	    ret++;
	return ret;
    };

  };

  union ShaderDataDatumBundle {
  public:
    ShaderResourceUsage usage;
    ShaderResourceProvider provider;
    constexpr ShaderDataDatumBundle() : provider(ShaderResourceProvider::last) {};
    constexpr ShaderDataDatumBundle(ShaderResourceProvider p) : provider(p) {};
    constexpr ShaderDataDatumBundle(bool write, ShaderStage stage,
				    ShaderResourceAccessType access = ShaderResourceAccessType::eUndefined) :
      usage(write, stage, access) {};
    constexpr ~ShaderDataDatumBundle() = default;
  };

  defineLiteralJaggedListAliasType(ShaderDataDatumBundle, 2, NormalizedShaderData);
#define passShaderData(SD) passLiteralJaggedList(SD)
#define acceptShaderData(SD) acceptLiteralJaggedListAliasXNS(::WITE::GPU, NormalizedShaderData, SD)

  template<acceptShaderData(JL)> constexpr ShaderData parseShaderData() {
    std::vector<ShaderResource> resources;
    std::vector<ShaderResourceUsage> usages;
    for(auto resource : JL) {
      usages.clear();
      for(auto usage : resource) {
	usages.push_back(usage.usage);
      }
      resources.emplace_back(resource.get().provider, usages);
    };
    return ShaderData(resources);
  };

  //TODO maybe make this generic
  template<acceptShaderData(D), ShaderResourceProvider P>
  constexpr auto SubsetShaderDataVolume() {
    std::array<size_t, D.DIM> ret;
    for(size_t i = 0;i < D.DIM;i++) ret[i] = 0;
    for(auto r : D) {
      auto p = r.get().provider;
      if(p != P) continue;
      ret[0]++;
      for(auto u : r) {
	ret[1]++;
      }
    }
    return ret;
  };

  template<acceptShaderData(D), ShaderResourceProvider P>
  constexpr auto SubsetShaderData() {
    constexpr auto V = SubsetShaderDataVolume<passLiteralJaggedList(D), P>();
    std::vector<Collections::StructuralPair<std::vector<ShaderResourceUsage>, ShaderResourceProvider>> ret;
    for(auto r : D) {
      auto p = r.get().provider;
      if(p != P) continue;
      auto& pair = ret.emplace_back();
      pair.v = P;
      for(auto u : r) {
	pair.k.push_back(u.get());
      }
    }
    return LJLA::NormalizedShaderData<V>(ret);
  };

  // static constexpr ShaderData emptyShaderData = ShaderData({});

  enum class PrimitiveNumberModel { eUnsigned, eTwosCompliment, eFloat, eFixed, eUnsignedFixed };

  template<PrimitiveNumberModel M, size_t S> struct PrimitiveNumber {};
  template<> struct PrimitiveNumber<PrimitiveNumberModel::eUnsigned, 1> { typedef uint8_t type; };
  template<> struct PrimitiveNumber<PrimitiveNumberModel::eUnsigned, 2> { typedef uint16_t type; };
  template<> struct PrimitiveNumber<PrimitiveNumberModel::eUnsigned, 4> { typedef uint32_t type; };
  template<> struct PrimitiveNumber<PrimitiveNumberModel::eUnsigned, 8> { typedef uint64_t type; };
  template<> struct PrimitiveNumber<PrimitiveNumberModel::eTwosCompliment, 1> { typedef int8_t type; };
  template<> struct PrimitiveNumber<PrimitiveNumberModel::eTwosCompliment, 2> { typedef int16_t type; };
  template<> struct PrimitiveNumber<PrimitiveNumberModel::eTwosCompliment, 4> { typedef int32_t type; };
  template<> struct PrimitiveNumber<PrimitiveNumberModel::eTwosCompliment, 8> { typedef int64_t type; };
  template<> struct PrimitiveNumber<PrimitiveNumberModel::eFloat, 4> { typedef float type; };
  template<> struct PrimitiveNumber<PrimitiveNumberModel::eFloat, 8> { typedef double type; };
  //TODO fill in some of these gaps, especially the fixed (normalized)
  template<PrimitiveNumberModel M, size_t S> using PrimitiveNumber_t = PrimitiveNumber<M, S>::type;

  struct VertexAspectSpecifier {
    size_t count;
    PrimitiveNumberModel model;
    size_t size;
    constexpr VertexAspectSpecifier(size_t c, PrimitiveNumberModel m, size_t s) : count(c), model(m), size(s) {};
    constexpr ~VertexAspectSpecifier() = default;
    constexpr vk::Format getFormat() const;
    friend constexpr bool operator<(const VertexAspectSpecifier& l, const VertexAspectSpecifier& r) {
      return l.count < r.count || (l.count == r.count && (l.size < r.size || (l.size == r.size && l.model < r.model)));
    };
  };

  const std::map<VertexAspectSpecifier, vk::Format> formatsBySizeTypeQty {
      { { 1, PrimitiveNumberModel::eUnsigned, 1 }, vk::Format::eR8Uint },
      { { 2, PrimitiveNumberModel::eUnsigned, 1 }, vk::Format::eR8G8Uint },
      { { 3, PrimitiveNumberModel::eUnsigned, 1 }, vk::Format::eR8G8B8Uint },
      { { 4, PrimitiveNumberModel::eUnsigned, 1 }, vk::Format::eR8G8B8A8Uint },
      { { 1, PrimitiveNumberModel::eUnsigned, 2 }, vk::Format::eR16Uint },
      { { 2, PrimitiveNumberModel::eUnsigned, 2 }, vk::Format::eR16G16Uint },
      { { 3, PrimitiveNumberModel::eUnsigned, 2 }, vk::Format::eR16G16B16Uint },
      { { 4, PrimitiveNumberModel::eUnsigned, 2 }, vk::Format::eR16G16B16A16Uint },
      { { 1, PrimitiveNumberModel::eUnsigned, 4 }, vk::Format::eR32Uint },
      { { 2, PrimitiveNumberModel::eUnsigned, 4 }, vk::Format::eR32G32Uint },
      { { 3, PrimitiveNumberModel::eUnsigned, 4 }, vk::Format::eR32G32B32Uint },
      { { 4, PrimitiveNumberModel::eUnsigned, 4 }, vk::Format::eR32G32B32A32Uint },
      { { 1, PrimitiveNumberModel::eUnsigned, 8 }, vk::Format::eR64Uint },
      { { 2, PrimitiveNumberModel::eUnsigned, 8 }, vk::Format::eR64G64Uint },
      { { 3, PrimitiveNumberModel::eUnsigned, 8 }, vk::Format::eR64G64B64Uint },
      { { 4, PrimitiveNumberModel::eUnsigned, 8 }, vk::Format::eR64G64B64A64Uint },
      { { 1, PrimitiveNumberModel::eTwosCompliment, 1 }, vk::Format::eR8Sint },
      { { 2, PrimitiveNumberModel::eTwosCompliment, 1 }, vk::Format::eR8G8Sint },
      { { 3, PrimitiveNumberModel::eTwosCompliment, 1 }, vk::Format::eR8G8B8Sint },
      { { 4, PrimitiveNumberModel::eTwosCompliment, 1 }, vk::Format::eR8G8B8A8Sint },
      { { 1, PrimitiveNumberModel::eTwosCompliment, 2 }, vk::Format::eR16Sint },
      { { 2, PrimitiveNumberModel::eTwosCompliment, 2 }, vk::Format::eR16G16Sint },
      { { 3, PrimitiveNumberModel::eTwosCompliment, 2 }, vk::Format::eR16G16B16Sint },
      { { 4, PrimitiveNumberModel::eTwosCompliment, 2 }, vk::Format::eR16G16B16A16Sint },
      { { 1, PrimitiveNumberModel::eTwosCompliment, 4 }, vk::Format::eR32Sint },
      { { 2, PrimitiveNumberModel::eTwosCompliment, 4 }, vk::Format::eR32G32Sint },
      { { 3, PrimitiveNumberModel::eTwosCompliment, 4 }, vk::Format::eR32G32B32Sint },
      { { 4, PrimitiveNumberModel::eTwosCompliment, 4 }, vk::Format::eR32G32B32A32Sint },
      { { 1, PrimitiveNumberModel::eTwosCompliment, 8 }, vk::Format::eR64Sint },
      { { 2, PrimitiveNumberModel::eTwosCompliment, 8 }, vk::Format::eR64G64Sint },
      { { 3, PrimitiveNumberModel::eTwosCompliment, 8 }, vk::Format::eR64G64B64Sint },
      { { 4, PrimitiveNumberModel::eTwosCompliment, 8 }, vk::Format::eR64G64B64A64Sint },
      { { 1, PrimitiveNumberModel::eFloat, 4 }, vk::Format::eR32Sfloat },
      { { 2, PrimitiveNumberModel::eFloat, 4 }, vk::Format::eR32G32Sfloat },
      { { 3, PrimitiveNumberModel::eFloat, 4 }, vk::Format::eR32G32B32Sfloat },
      { { 4, PrimitiveNumberModel::eFloat, 4 }, vk::Format::eR32G32B32A32Sfloat },
      { { 1, PrimitiveNumberModel::eFloat, 8 }, vk::Format::eR64Sfloat },
      { { 2, PrimitiveNumberModel::eFloat, 8 }, vk::Format::eR64G64Sfloat },
      { { 3, PrimitiveNumberModel::eFloat, 8 }, vk::Format::eR64G64B64Sfloat },
      { { 4, PrimitiveNumberModel::eFloat, 8 }, vk::Format::eR64G64B64A64Sfloat },
    };

#define acceptVertexModel(VM, NOM) size_t NOM## _SIZE, std::array<NOM## _SIZE, VertexAspectSpecifier> NOM
#define passVertexModel(VM) NOM .size(), NOM

  // template<size_t CNT, PrimitiveNumberModel M, size_t S> struct VertexAspect {
  //   static_assert(CNT>0, "illegal count");
  //   typedef PrimitiveNumber_t<M, S> T;
  //   union {
  //     std::array<T, CNT> data;
  //     // struct { T value; VertexAspect<CNT-1, M, S> next; };
  //     glm::vec<CNT, T, glm::qualifier::packed> glmvec;
  //   };
  //   static_assert(sizeof(data) == sizeof(glmvec));
  //   // static_assert(sizeof(data) == sizeof(T) + sizeof(next));
  //   //TODO some operators (alias glmvec)
  //   //TODO to vk::Format
  // };
  // template<PrimitiveNumberModel M, size_t S> struct VertexAspect<1, M, S> {
  //   typedef PrimitiveNumber_t<M, S> T;
  //   T value;
  // };
  template<size_t CNT, PrimitiveNumberModel M, size_t S> using VertexAspect =
    glm::vec<CNT, PrimitiveNumber_t<M, S>, glm::qualifier::packed>;//abstracted so an extension can be made later
  template<VertexAspectSpecifier U> using VertexAspect_t = VertexAspect<U.count, U.model, U.size>;

  template<acceptVertexModel(M)> struct Vertex {
    static constexpr size_t attributes = M_SIZE;
    typedef VertexAspect_t<M[0]> T;
    T value;
    Vertex<attributes-1, subArray<1, attributes-1>(M)> rest;
    template<size_t SKIP> auto& sub() { if constexpr(SKIP == 0) return value; else return rest.template sub<SKIP-1>(); };
    template<class A, class... B> Vertex(A a, B... b) : value(a), rest(std::forward<B...>(b...)) {};
  };

  template<VertexAspectSpecifier T> struct Vertex<1, std::array<1, VertexAspectSpecifier>{T}> {
    T value;
    static constexpr size_t attributes = 1;
    template<size_t SKIP> auto sub() { static_assert(SKIP == 0, "SKIP was out of bounds"); return value; };
    template<class U> Vertex(U u) : value(u) {};
  };

}

