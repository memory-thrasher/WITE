#pragma once

#include <algorithm>
#include <compare>

#include "LiteralMap.hpp"
#include "StdExtensions.hpp"
#include "Image.hpp"
#include "Buffer.hpp"
#include "StructuralMap.hpp"
#include "ConstevalArray.hpp"

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
    bool write;
    ShaderStage stage;
    ShaderResourceAccessType access;

    constexpr ShaderResourceUsage() = default;
    constexpr ShaderResourceUsage(const bool write, const ShaderStage stage,
				  const ShaderResourceAccessType access = ShaderResourceAccessType::eUndefined) :
      write(write), stage(stage), access(access) {};
    constexpr ~ShaderResourceUsage() = default;
    constexpr ShaderResourceUsage(const ShaderResourceUsage& o) = default;

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

    consteval vk::DescriptorType getVkDSType() const {
      switch(access) {
      case ShaderResourceAccessType::eUniform: return write ? vk::DescriptorType::eStorageBuffer : vk::DescriptorType::eUniformBuffer;
      case ShaderResourceAccessType::eUniformTexel: return write ? vk::DescriptorType::eStorageTexelBuffer : vk::DescriptorType::eUniformTexelBuffer;
      case ShaderResourceAccessType::eSampled: return write ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eSampledImage;
      default: ASSERT_CONSTEXPR_RET(0, {}); break;
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
    Collections::LiteralList<ShaderResourceUsage> usage;
    ShaderResourceProvider provider;
    constexpr ShaderResource() = default;
    constexpr ShaderResource(const ShaderResource&) = default;
    consteval ShaderResource(const ShaderResourceProvider provider, const Collections::LiteralList<ShaderResourceUsage>& usage) :
      usage(usage), provider(provider) {};
    consteval ShaderResource(// const ShaderResourceType type, 
			     const Collections::LiteralList<ShaderResourceUsage> usage,
			     const ShaderResourceProvider provider) :
      // type(type),
      usage(usage),
      provider(provider)
    {};

    constexpr auto begin() const { return usage.begin(); };
    constexpr auto end() const { return usage.end(); };

    template<class Resource> struct acceptsTest { constexpr bool operator()(const ShaderResource*) { return false; }; };

    template<ImageSlotData ISD> struct acceptsTest<Image<ISD>> {
      constexpr bool operator()(const ShaderResource* s) {
	for(auto& u : s->usage) {
	  switch(u.access) {
	  // case ShaderResourceAccessType::eUndefined:
	  //   switch(u.stage) {
	  //   case ShaderStage::eDraw: if(!(ISD.usage & ImageBase::USAGE_INDIRECT)) return false; break;
	  //   case ShaderStage::eAssembler: break;
	  //   default: return false; break;
	  //   }
	  //   break;
	  // case ShaderResourceAccessType::eVertex: if(!(ISD.usage & ImageBase::USAGE_VERTEX)) return false; break;
	  // case ShaderResourceAccessType::eUniform:
	  // case ShaderResourceAccessType::eUniformTexel:
	  case ShaderResourceAccessType::eSampled:
	    if(!u.write && !(ISD.usage & (ImageBase::USAGE_DS_READ | ImageBase::USAGE_DS_SAMPLED |
					  ImageBase::USAGE_ATT_INPUT))) return false;
	    if(u.write && !(ISD.usage & ImageBase::USAGE_DS_WRITE)) return false;
	    break;
	  case ShaderResourceAccessType::eDepthAtt: if(!(ISD.usage & ImageBase::USAGE_ATT_DEPTH)) return false; break;
	  case ShaderResourceAccessType::eColorAtt: if(!(ISD.usage & ImageBase::USAGE_ATT_OUTPUT)) return false; break;
	  default: return false; break;
	  }
	}
	return true;
      };
    };

    template<BufferSlotData BSD> struct acceptsTest<Buffer<BSD>> {
      constexpr bool operator()(const ShaderResource* s) {
	for(auto u : s->usage) {
	  switch(u.access) {
	  case ShaderResourceAccessType::eUndefined:
	    switch(u.stage) {
	    case ShaderStage::eDraw: if(!(BSD.usage & ImageBase::USAGE_INDIRECT)) return false; break;
	    case ShaderStage::eAssembler: break;
	    default: return false; break;
	    }
	    break;
	  case ShaderResourceAccessType::eVertex: if(!(BSD.usage & ImageBase::USAGE_VERTEX)) return false; break;
	  case ShaderResourceAccessType::eUniform:
	  case ShaderResourceAccessType::eUniformTexel:
	  // case ShaderResourceAccessType::eSampled:
	  //   if(!u.write && !(BSD.usage & (ImageBase::USAGE_DS_READ | ImageBase::USAGE_DS_SAMPLED |
	  // 				  ImageBase::USAGE_ATT_INPUT))) return false;
	  //   if(u.write && !(BSD.usage & ImageBase::USAGE_DS_WRITE)) return false;
	  //   break;
	  // case ShaderResourceAccessType::eDepthAtt: if(!(ISD.usage & ImageBase::USAGE_ATT_DEPTH)) return false; break;
	  // case ShaderResourceAccessType::eColorAtt: if(!(ISD.usage & ImageBase::USAGE_ATT_OUTPUT)) return false; break;
	  default: return false; break;
	  }
	}
	return true;
      };
    };

    //TODO ^for buffers?
    template<class Resource> constexpr bool accepts() { return acceptsTest<Resource>::accepts(this); };

    //guaranteed unique
    constexpr uint64_t hashCode() const {
      uint64_t ret = static_cast<uint64_t>(provider);
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

    Collections::LiteralList<ShaderResource> resources;
    constexpr ShaderData() = default;
    constexpr ~ShaderData() = default;
    constexpr ShaderData(const ShaderData&) = default;
    consteval ShaderData(const Collections::LiteralList<ShaderResource> resources) : resources(resources) {};
    constexpr ShaderData& operator=(const ShaderData&) = default;
    consteval const ShaderResource& operator[](const size_t i) const {
      return resources[i];
    };

    constexpr auto begin() const {
      return resources.begin();
    };

    constexpr auto end() const {
      return resources.end();
    };

    typedef uint64_t hashcode_t;//used in maps to cache descriptor info
    //uniqueness likely but not gauranteed
    constexpr hashcode_t hashCode() const {
      uint64_t ret = 0;
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

    consteval size_t countDescriptorSetResources() const {
      size_t ret = 0;
      std::vector<vk::DescriptorType> types;
      for(ShaderResource r : resources) {
	types.clear();
	for(ShaderResourceUsage cu : r.usage) {
	  if(cu.isDS() && !Collections::contains(types, cu.getVkDSType())) {
	    ret++;
	    types.push_back(cu.getVkDSType());
	  }
	}
      }
      return ret;
    };

    template<size_t retCount> consteval auto getDescriptorSetLayoutBindings() const {
      Collections::StructuralMap<vk::DescriptorType, vk::ShaderStageFlags> flags;
      Collections::ConstevalArray<vk::DescriptorSetLayoutBinding, retCount> ret;
      if constexpr(retCount > 0) {
	uint32_t i = 0;
	for(ShaderResource r : resources) {
	  flags.clear();
	  for(ShaderResourceUsage cu : r.usage)
	    if(cu.isDS())
	      flags[cu.getVkDSType()] |= cu.getVkStages();
	  for(auto p : flags) {
	    ret[i] = { i, p.k, 1, p.v };
	    i++;
	  }
	}
      }
      return ret;
    };

    // template<ShaderData SD> static consteval auto getDescriptorSetLayoutBindings() const {
    //   return SD::getDescriptorSetLayoutBindings<SD::countDescriptorSetResources()>();
    // };

    template<size_t bindingCount> //must always = countDescriptorSetResources()
    consteval void poolSizesHelper(Collections::StructuralMap<vk::DescriptorType, uint32_t>& ret) const {
      auto bindings = getDescriptorSetLayoutBindings<bindingCount>();
      for(auto binding : bindings)
	ret[binding.descriptorType]++;
    };

    template<size_t bindingCount> //must always = countDescriptorSetResources()
    consteval uint32_t countDistinctTypes() const {
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

  constexpr ShaderData ShaderData_EMPTY = {};

  template<ShaderData D, ShaderResourceProvider P>
  consteval auto SubsetShaderDataVolume() {
    std::array<size_t, 2> ret { 0, 0 };
    for(auto r : D) {
      auto p = r.provider;
      if(p != P) continue;
      ret[0]++;
      ret[1] += r.usage.len;
    }
    return ret;
  };

  template<std::array<size_t, 2> VOL> struct ShaderDataStorage {
    std::array<ShaderResourceUsage, VOL[1]> allUsages;
    std::array<ShaderResource, VOL[0]> allResources;
    ShaderData data;
    constexpr ShaderDataStorage() = default;
    constexpr ShaderDataStorage(const ShaderDataStorage<VOL>&o) :
      allUsages(o.allUsages), allResources(o.allResources), data({ allResources.data(), VOL[0] }) {};
    constexpr ~ShaderDataStorage() = default;
    template<class C> requires std::is_trivially_copyable_v<C> consteval ShaderDataStorage(C rc) : ShaderDataStorage(rc.begin(), rc.end()) {};
    template<class C> requires (!std::is_trivially_copyable_v<C>) consteval ShaderDataStorage(const C& rc) : ShaderDataStorage(rc.begin(), rc.end()) {};
    template<class Iter> consteval ShaderDataStorage(Iter begin, Iter end) {
      size_t ui = 0, ri = 0;
      auto rt = begin;
      while(rt != end) {
	auto& uc = rt->k;
	auto ut = uc.begin();
	size_t cnt = 0;
	while(ut != uc.end()) {
	  allUsages[ui++] = *ut;
	  cnt++;
	  ut++;
	}
	allResources[ri++] = { { &allUsages[ui - cnt], cnt }, rt->v };
	rt++;
      }
      data = Collections::LiteralList<ShaderResource>(allResources.data(), VOL[0]);
    };
    constexpr inline operator ShaderData() const { return data; };
  };

  template<ShaderData D, ShaderResourceProvider P, std::array<size_t, 2> V = SubsetShaderDataVolume<D, P>()>
  consteval ShaderDataStorage<V> SubsetShaderData() {
    std::vector<Collections::StructuralPair<std::vector<ShaderResourceUsage>, ShaderResourceProvider>> ret;
    for(auto r : D) {
      auto p = r.provider;
      if(p != P) continue;
      auto& pair = ret.emplace_back();
      pair.v = P;
      for(auto u : r) {
	pair.k.push_back(u);
      }
    }
    ShaderDataStorage<V> ret2 { ret };
    return ret2;
  };

  template<collection C1 = std::initializer_list<Collections::StructuralPair<std::initializer_list<ShaderResourceUsage>, ShaderResourceProvider>>> consteval std::array<size_t, 2> getShaderDataVolume(const C1& rc) {
    size_t ui = 0, ri = 0;
    auto rt = rc.begin();
    while(rt != rc.end()) {
      auto& uc = rt->k;
      auto ut = uc.begin();
      while(ut != uc.end()) {
	ui++;
	ut++;
      }
      ri++;
      rt++;
    }
    return { ri, ui };
  };

#define defineShaderData(NOM, ...) constexpr ShaderDataStorage<::WITE::GPU::getShaderDataVolume({ __VA_ARGS__ })>NOM{std::initializer_list<Collections::StructuralPair<std::initializer_list<ShaderResourceUsage>, ShaderResourceProvider>>{ __VA_ARGS__ }};

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
    constexpr VertexAspectSpecifier() = default;
    constexpr ~VertexAspectSpecifier() = default;
    constexpr vk::Format getFormat() const;
    constexpr inline size_t totalSize() const { return size * count; };
    friend constexpr bool operator<(const VertexAspectSpecifier& l, const VertexAspectSpecifier& r) {
      return l.count < r.count || (l.count == r.count && (l.size < r.size || (l.size == r.size && l.model < r.model)));
    };
  };

  defineLiteralMap(VertexAspectSpecifier, vk::Format, formatsBySizeTypeQty,
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
    );

  template<size_t CNT, PrimitiveNumberModel M, size_t S> using VertexAspect =
    glm::vec<CNT, PrimitiveNumber_t<M, S>, glm::qualifier::packed>;//abstracted so an extension can be made later
  template<VertexAspectSpecifier U> using VertexAspect_t = VertexAspect<U.count, U.model, U.size>;

  constexpr inline vk::Format VertexAspectSpecifier::getFormat() const {
    return formatsBySizeTypeQty[*this];
  };

  using VertexModel = Collections::LiteralList<VertexAspectSpecifier>;

#define defineVertexModel(NOM, ...) defineLiteralList(VertexAspectSpecifier, NOM, __VA_ARGS__ )

  namespace VertexPrefab {
    constexpr VertexAspectSpecifier
    dXYZ = { 3, PrimitiveNumberModel::eFloat, 8 };
    defineVertexModel(basic3d, dXYZ);
  };

  template<VertexModel M, size_t L = M.len> struct Vertex {
    static constexpr size_t attributes = M.len;
    typedef VertexAspect_t<M[0]> T;
    T value;
    Vertex<M.sub(1)> rest;
    template<size_t SKIP> constexpr auto& sub() {
      if constexpr(SKIP == 0)
	return value;
      else
	return rest.template sub<SKIP-1>();
    };
    template<class... B> constexpr Vertex(T a, B... b) : value(a), rest(std::forward<B...>(b...)) {};
    constexpr Vertex(T a) : value(a) {};
  };

  template<VertexModel M> requires(M.len == 0) struct Vertex<M, 0> {
    static constexpr size_t attributes = 0;
  };

}

