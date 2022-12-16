#pragma once

#include "ShaderData.hpp"
#include "PerGpuPerThread.hpp"
#include "Vulkan.hpp"
#include "FrameSwappedResource.hpp"

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
    constexpr static uint32_t typeCount = D.countDistinctTypes<resourceCount>();
    constexpr static vk::DescriptorPoolSize types[typeCount] = D.getDescriptorPoolSizes<typeCount, resourceCount>();
    constexpr static vk::DescriptorPoolCreateInfo poolCI { {}, setsPerPool, typeCount, types };
  };

  class ShaderDescriptorLifeguard { //manager of the pool. ha.
  private:
    static Collections::PerGpuPerThread<std::unique_ptr<std::map<ShaderData::hashcode_t, ShaderDescriptorLifeguard>>> all;

    std::vector<vk::DescriptorPool> poolPool;
    std::stack<vk::DescriptorSet> free;//MAY contains DescriptorSets from a pool NOT associated with this thread, though it will have an identical layout.
    vk::DescriptorSetLayout dsl;
    const vk::DescriptorPoolCreateInfo* poolCI;
    Gpu* gpu;
    size_t currentPool = 0;

    ShaderDescriptorLifeguard() = delete;
    ShaderDescriptorLifeguard(ShaderDescriptorLifeguard&) = delete;
    ShaderDescriptorLifeguard(const vk::DescriptorSetLayoutCreateInfo* dslCI,
			      const vk::DescriptorPoolCreateInfo* poolCI,
			      size_t gpuIdx);
    void allocatePool();
    vk::DescriptorSet allocate();
    void deallocate(vk::DescriptorSet r);

    template<ShaderData FD>
    static ShaderDescriptorLifeguard* get(size_t gpuIdx) {
      static constexpr auto hc = FD.hashCode();
      auto map = all[gpuIdx];
      ShaderDescriptorLifeguard* sdl;
      if(!map->contains(hc)) {
	sdl = map->emplace(FD, &ShaderDescriptorPoolLayout<FD>::dslCI, &ShaderDescriptorPoolLayout<FD>::poolCI, gpuIdx).first;
      } else {
	sdl = &map->operator[](hc);
      }
      return sdl;
    };

  public:
    template<ShaderData D, ShaderResourceProvider P>
    static void allocate(vk::DescriptorSet* ret, size_t gpuIdx) {
      *ret = get<D.SubsetFrom(P)>(gpuIdx)->allocate();
    };

    template<ShaderData D, ShaderResourceProvider P>
    static void deallocate(size_t gpuIdx, vk::DescriptorSet* r) {
      get<D.SubsetFrom(P)>(gpuIdx)->deallocate(*r);
    };

    template<ShaderData D, ShaderResourceProvider P>
    static vk::DescriptorSetLayout getDSLayout(size_t gpuIdx) {
      return get<D.SubsetFrom(P)>(gpuIdx)->dsl;
    };

  };

  #error TODO make this frameswap-aware or exclusive
  template<ShaderData D, ShaderResourceProvider P> class ShaderDescriptor {
  private:
    static constexpr ShaderData FD = D.SubsetFrom(P);
    static constexpr auto FDC = FD.hashCode();

    struct DescriptorSetContainer {
      vk::DescriptorSet ds;
      uint64_t lastUpdated;
      Util::SyncLock lock;
      DescriptorSetContainer() : ds(ShaderDescriptorLifeguard::allocate<D, P>()) {
      };
      ~DescriptorSetContainer() {
	ShaderDescriptorLifeguard::deallocate<D, P>(ds);
      };
    };

    struct perGpu {
      Util::FrameSwappedResource<DescriptorSetContainer, 3> ds;
    };

    PerGpu<std::unique_ptr<perGpu>> data;
    std::atomic_uint64_t lastResourceUpdated;
    static constexpr uint32_t resourceCount = ShaderDescriptorPoolLayout<FD>::resourceCount;
    GpuResource* resources[resourceCount];

    template<size_t idx, class R1, class R2, class... R> inline void SetResource(R1 r1, R2 r2, R... resources) {
      SetResource<idx>(r1);
      SetResource<idx + 1>(r2, std::forward<R...>(resources...));
    };

    static void makePG(std::unique_ptr<perGpu>* ret, size_t gpu) {
      *ret = std::make_unique<perGpu>();
    };

    void markUpdated() {
      uint64_t frame = Util::FrameCounter::getFrame();
      uint64_t oldValue = lastResourceUpdated;
      while(oldValue < frame && !lastResourceUpdated.compare_exchange_weak(oldValue, frame));
    };

  public:
    static constexpr bool empty = false;
    ShaderDescriptor(ShaderDescriptor&) = delete;
    ShaderDescriptor() : data(decltype(data)::creator_t_F::make(&ShaderDescriptor<D, P>::makePG)) {};

    //gets the READABLE DS. Will first bind resources against it if out of date.
    inline vk::DescriptorSet get(size_t gpu) {
      uint64_t ldu = lastResourceUpdated,
	frame = Util::FrameCounter::getFrame();
      auto& dsc = data.get(gpu).ds.get();
      if(dsc.lastUpdated < frame - 1 && dsc.lastUpdated < ldu) {
	Util::ScopeLock lock(dsc.lock);
	if(dsc.lastUpdated < frame - 1 && dsc.lastUpdated < ldu) {
	  vk::WriteDescriptorSet dsw[resourceCount];
	  for(uint32_t i = 0;i < resourceCount;i++) {
	    dsw[i].dstSet = dsc.ds;
	    dsw[i].dstBinding = i;
	    //populated by resource.populateDSWrite: arrayElement, descriptorCount, p*Info
	    dsw[i].descriptorType = ShaderDescriptorPoolLayout<FD>::dsBindings[i].descriptorType;
	    resources[i].populateDSWrite(&dsw[i], gpu);
	  }
	  Gpu::get(gpu).getVkDevice().updateDescriptorSets(resourceCount, &dsw, 0, NULL);
	  dsc.lastUpdated = ldu;
	}
      }
      return dsc.ds;
    };

    template<size_t idx, class R1> inline void SetResource(R1 resource) {
      static_assert(FD.accepts<R1>());
      ASSERT_TRAP(resource, "null resource given to descriptor");
      resources[idx] = resource;
      makrUpdated();
    };

    template<class... R> inline void SetResources(R... resources) {
      SetResource<0>(std::forward<R...>(resources...));
    };
  };

  template<ShaderResourceProvider P> class ShaderDescriptor<{}, P> {//empty D
    static constexpr bool empty = true;
  };

};

