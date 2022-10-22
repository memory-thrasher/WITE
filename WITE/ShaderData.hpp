#include <vector>
#include <algorithm>

namespace WITE::Gpu {

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
  };

  class ShaderResource {
    // const ShaderResourceType type;
    const std::vector<ShaderResourceUsage> usage;
    ShaderResource() = delete;
    constexpr ShaderResource(// const ShaderResourceType type, 
			     const std::initializer_list<ShaderResourceUsage> usage) :
      // type(type), 
      usage(usage) {};
  };

  struct ShaderData {
  public:
    static constexpr ShaderResourceUsage Indirect = { false, ShaderStage::eDraw },
      Index = { false, ShaderStage::eAssembler },
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
      DepthAttachmentRO = { false, ShaderStage::eFragmentDepth, Accesstype::eDepthAtt },
      DepthAttachment = { true, ShaderStage::eFragmentDepth, Accesstype::eDepthAtt },
    //Blending
      ColorAttachmentRO = { false, ShaderStage::eBlending, Accesstype::eColorAtt },
      ColorAttachment = { true, ShaderStage::eBlending, Accesstype::eColorAtt },
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

    const std::vector<ShaderResource> resources;
    constexpr ShaderData(const std::initializer_list<ShaderResource> resources) : resources(resources) {};

    constexpr bool contains(const ShaderResourceUsage u) {
      return std::any_of(resources.cbegin(), resources.cend(),
			 [u](ShaderResource r) {
			   return std::any_of(r.usage.cbegin(), r.usage.cend(),
					      [u](ShaderResourceUsage cu) {
						return cu == u;
					      });
			 });
    };

  };

}
