#include <algorithm>

#include "StructuralConstList.hpp"
#include "StdExtensions.hpp"
#include "Image.hpp"

namespace WITE::GPU {

  // enum class ShaderResourceType { eBuffer, eImage };
  enum class ShaderStage { eDraw, eAssembler, eVertex, eTessellation, eGeometry, eFragmentDepth, eFragment, eBlending,
			   eTask, eMesh, eCompute };
  enum class ShaderResourceAccessType { eUndefined, eVertex, eUniform, eUniformTexel, eSampled, eDepthAtt, eColorAtt };

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
    constexpr bool operator==(const ShaderResourceUsage& a) {
      return a.write == this->write && a.stage == this->stage && a.access == this->access;
    };
  };

  class ShaderResource {
  public:
    // const ShaderResourceType type;
    const Collections::StructuralConstList<ShaderResourceUsage> usage;
    ShaderResource() = delete;
    constexpr ShaderResource(const ShaderResource&) = default;
    constexpr ShaderResource(// const ShaderResourceType type, 
			     const std::initializer_list<ShaderResourceUsage> usage) :
      // type(type), 
      usage(usage) {};
    template<class Resource> struct acceptsTest { constexpr bool operator()(const ShaderResource*) { return false; }; };
    template<ImageSlotData ISD> struct acceptsTest<Image<ISD>> {
      constexpr bool operator()(const ShaderResource* s) {
	return s->usage.every([](const ShaderResourceUsage u) {
	  switch(u.access) {//TODO
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
	  return true;
	});
      };
    };
    template<class Resource> constexpr bool accepts() { return acceptsTest<Resource>::accepts(this); };
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
    const ShaderResource& operator[](const size_t i) const {
      return resources[i];
    };

    constexpr bool contains(const ShaderResourceUsage u) const {
      return std::any_of(resources.cbegin(), resources.cend(),
			 [u](ShaderResource r) {
			   return std::any_of(r.usage.cbegin(), r.usage.cend(),
					      [u](ShaderResourceUsage cu) {
						return cu == u;
					      });
			 });
    };

    constexpr bool contains(const ShaderStage stage) const {
      return std::any_of(resources.cbegin(), resources.cend(),
			 [stage](ShaderResource r) {
			   return std::any_of(r.usage.cbegin(), r.usage.cend(),
					      [stage](ShaderResourceUsage cu) {
						return cu.stage == stage;
					      });
			 });
    };

    constexpr bool contains(const ShaderResourceAccessType access) const {
      return std::any_of(resources.cbegin(), resources.cend(),
			 [access](ShaderResource r) {
			   return std::any_of(r.usage.cbegin(), r.usage.cend(),
					      [access](ShaderResourceUsage cu) {
						return cu.access == access;
					      });
			 });
    };

    constexpr bool containsWritable() const {
      return std::any_of(resources.cbegin(), resources.cend(),
			 [](ShaderResource r) {
			   return std::any_of(r.usage.cbegin(), r.usage.cend(),
					      [](ShaderResourceUsage cu) {
						return cu.write;
					      });
			 });
    };

    constexpr bool containsOnly(const std::initializer_list<ShaderStage> stages) const {
      return std::none_of(resources.cbegin(), resources.cend(),
			 [stages](ShaderResource r) {
			   return std::any_of(r.usage.cbegin(), r.usage.cend(),
					      [stages](ShaderResourceUsage cu) {
						return !Collections::contains(stages, cu.stage);
					      });
			 });
    };

  };

}
