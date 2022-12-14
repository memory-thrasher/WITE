#pragma once

#include "ShaderData.hpp"
#include "PerGpuPerThread.hpp"
#include "Vulkan.hpp"

/*
one descriptor set per provider type PerGpu.
Each provider will store their descriptor for their lifetime.
Pool bucket PerGpuPerThread.
None of the pool buckets should be specific to the entire ShaderData object.
Frameswapped DS PerGpu should be all a provider needs, allocated on constructor.
Renderable check compatibility with associated shader pipeline, bind at each render.
RenderTarget binds at start of render (verify binding before the pipeline is possible, or else rebind in renderto)
Shader instance bind after the pipeline.
 */

namespace WITE::GPU {

  template<ShaderData D> struct ShaderDescriptorPoolLayout {
    constexpr static uint32_t setsPerPool = 100;
    constexpr static uint32_t resourceCount = D.countDescriptorSetResources();
    constexpr static vk::DescriptorSetLayoutBinding dsBindings[resourceCount] = D.getDescriptorSetLayoutBindings<resourceCount>();
    constexpr static vk::DescriptorSetLayoutCreateInfo dslCI { {}, resourceCount, dsBindings };
    constexpr static uint32_t typeCount = D.countDistinctTypes();
    constexpr static vk::DescriptorPoolSize types[typeCount] = D.getDescriptorPoolSizes();
    constexpr static vk::DescriptorPoolCreateInfo poolCI { {}, setsPerPool, typeCount, types };
  };

  class ShaderDescriptorLifeguard { //manager of the pool. ha.
  private:
    static Collections::PerGpuPerThread<std::map<ShaderData, ShaderDescriptorLifeguard>> all;

    std::vector<vk::DescriptorPool> poolPool;
    std::stack<vk::DescriptorSet> free;//MAY contains DescriptorSets from a pool NOT associated with this thread, though it will have an identical layout.
    vk::DescriptorSetLayout dsl;
    const vk::DescriptorPoolCreateInfo* poolCI;
    Gpu* gpu;
    size_t currentPool = 0;

    ShaderDesctorLifeguard() = delete;
    ShaderDesctorLifeguard(ShaderDesctorLifeguard&) = delete;
    ShaderDesctorLifeguard(const vk::DescriptorSetLayoutCreateInfo* dslCI,
			   const vk::DescriptorPoolCreateInfo* poolCI,
			   size_t gpuIdx);
    void allocatePool();
    vk::DescriptorSet allocate();
    void free(vk::DescriptorSet r);

  public:
    template<ShaderData D, ShaderResourceProvider P>
    static void allocate(vk::DescriptorSet* ret, size_t gpuIdx) {
      static constexpr ShaderData FD = D.SubsetFrom(P);
      auto& map = all[gpuIdx];
      ShaderDescriptorLifeguard& sdl
      if(!map.contains(FD)) {
	sdl = *map.emplace(FD, &ShaderDescriptorPoolLayout<FD>::dslCI, &ShaderDescriptorPoolLayout<FD>::poolCI, gpuIdx).first;
      } else {
	sdl = map[FD];
      }
      *ret = sdl.allocate();
    };

    template<ShaderData D, ShaderResourceProvider P, ShaderData FD = D.SubsetFrom(P)>
    static void deallocate(size_t gpuIdx, vk::DescriptorSet* r) {
      all[gpuIdx][FD].free(*r);
    };

  };

  template<ShaderData D, ShaderResourceProvider P> class ShaderDescriptor {
  private:
    static constexpr ShaderData FD = D.SubsetFrom(P);
    struct perGpu {
      Util::FrameSwappedResource<vk::DescriptorSet, 3> ds;
      uint64_t lastUpdated;
      perGpu() : ds({ ShaderDescriptorLifeguard::allocate<D, P>(),
	  ShaderDescriptorLifeguard::allocate<D, P>(), ShaderDescriptorLifeguard::allocate<D, P>() })
      {};
      ~perGpu() {
	for(auto ds : ds.all()) ShaderDescriptorLifeguard::deallocate<D, P>(ds);
      };
    };
    PerGpu<std::unique_ptr<perGpu>> data;
    uint64_t lastResourceUpdated;
    GpuResource* resources[ShaderDescriptorPoolLayout<FD>.resourceCount];
    template<size_t idx, class R1, class R2, class... R> inline void SetResource(R1 r1, R2 r2, R... resources) {
      SetResource<idx>(r1);
      SetResource<idx + 1>(r2, std::forward<R...>(resources...));
    };
    static void makePG(std::unique_ptr<perGpu>* ret, size_t gpu) {
      *ret = std::make_unique<perGpu>();
    };
  public:
    static constexpr bool empty = false;
    ShaderDescriptor(ShaderDescriptor&) = delete;
    ShaderDescriptor() : ds(decltype(ds)::creator_t_F::make(&ShaderDescriptor<D, P>::makePG)) {};
    inline vk::DescriptorSet get(size_t gpu) { return ds.get(gpu); };
    template<size_t idx, class R1> inline void SetResource(R1 resource) {
      static_assert(FD.accepts<R1>());
      resources[idx] = resource;
      lastResourceUpdated = Util::FrameCounter::getFrame();
    };
    template<class... R> inline void SetResources(R... resources) {
      SetResource<0>(std::forward<R...>(resources...));
    };
  };

  template<ShaderResourceProvider P> class ShaderDescriptor<{}, P> {//empty D
    static constexpr bool empty = true;
  };

};

