#include "RenderTarget.hpp"
#include "VulkanConverters.hpp"
#include "Image.hpp"

namespace WITE::GPU {

  struct BackedRenderTargetData {
    Collections::LiteralList<GpuResourceSlotInfo> resources;
    Collections::LiteralList<RenderStep> steps;
    constexpr ~BackedRenderTargetData() = default;
    constexpr BackedRenderTargetData(Collections::LiteralList<GpuResourceSlotInfo> rez,
				     Collections::LiteralList<RenderStep> steps) :
      resources(rez), steps(steps) {};
  };

  //TODO if there are attachments, and STEPS contains a compute step, add DS storage image access to the attachments
  template<BackedRenderTargetData DATA, logicalDeviceMask_t LDM>
  class BackedRenderTarget : public RenderTarget {
  public:
    typedef BackedRenderTarget<DATA, LDM> SPECULUM;
    static constexpr size_t resourceCount = DATA.resources.len;
    static constexpr size_t stepCount = DATA.steps.len;
    static constexpr std::array<GpuResourceSlotInfo, resourceCount> REZ = subArray<0, resourceCount>(DATA.resources);
    static constexpr std::array<RenderStep, stepCount> STEPS = subArray<0, stepCount>(DATA.steps);
  protected:
    virtual const Collections::IteratorWrapper<const RenderStep*> getRenderSteps() const override { return { DATA.steps }; };
    virtual void getRenderInfo(WorkBatch::renderInfo* out, size_t gpuIdx) override {
      out->rpbi = { renderPasses.get(gpuIdx), fbs.get(gpuIdx)->getWrite(), { { 0, 0 }, { width, height } },
	resourceCount, clears };
      out->resourceCount = resourceCount;
      static_assert(resourceCount <= MAX_RESOURCES);
      for(size_t i = 0;i < resourceCount;i++) {
	out->resources[i] = resources[i].swapper.getWrite();
	out->initialLayouts[i] = attachments[i].initialLayout;
	out->finalLayouts[i] = attachments[i].finalLayout;
      }
      out->sc = vk::SubpassContents::eInline;
    };
  private:
    uint32_t width, height;//TODO resize function (destroy FBs, keep render pass)
    //great length to avoid new-allocation of sub-resources
    struct resource_bt {
      uint32_t firstWriteStep, lastWriteStep, firstReadStep, lastReadStep;
      Util::FrameSwappedResource<ImageBase*> swapper;
      ImageBase* staging;
      void* hostRam;
      resource_bt(std::array<ImageBase*, 2>& data, ImageBase* staging) : swapper(data, NULL), staging(staging) {};
      virtual ~resource_bt() { if(hostRam) free(hostRam); };
    };
    template<size_t i> struct resource_ct : public resource_bt {
      static_assert(REZ[i].type == GpuResourceType::eImage,
		    "only images can be attached, and only attachments are provided by the RP.");
      static_assert((REZ[i].imageData.usage & GpuResource::USAGE_ATT_INPUT) == 0,
		    "NYI feedback loop, requires device feature test");
      Image<REZ[i].externallyStagedResourceSlot().imageData> rez[2];
      Image<REZ[i].stagingResourceSlot().imageData> stagingRez;
      //all gpu resources shall include a PerGpu and manage any scynchronization between them
      std::array<GpuResource*, 2> rezPtr { &rez[0], &rez[1] };
      resource_ct() : resource_bt(rezPtr, &stagingRez) {};
    };
    template<size_t i> struct resource_t {
      resource_ct<i> rezContainer;
      resource_t<i-1> next;
      inline constexpr resource_bt* get(size_t j) { return i == j ? &rezContainer : next.get(j); };
      template<size_t j> inline resource_ct<j>* get() { return reinterpret_cast<resource_ct<j>*>(get(j)); };
    };
    template<> struct resource_t<0> {
      resource_ct<0> rezContainer;
      inline constexpr resource_bt* get(size_t j) { return &rezContainer; };
      template<size_t j> inline resource_ct<j>* get() { &rezContainer; };
    };
    struct stepAttachmentReferences {
      std::array<vk::AttachmentReference2, resourceCount> inputs, colors, depths;//TODO resolve?
      std::array<uint32_t, resourceCount> preserves;
      uint32_t inputCount, colorCount, depthCount, preserveCount;
    };
    resource_t<resourceCount-1> resourceStorage;
    std::array<resource_bt*, resourceCount> resources;
    std::array<vk::AttachmentDescription2, resourceCount> attachments;
    std::array<vk::SubpassDescription2, stepCount> subpasses;
    std::array<vk::SubpassDependency2, (stepCount-1)*resourceCount*2> depends;//might not all be used, track size seperately
    size_t dependsCount = 0;
    std::array<stepAttachmentReferences, stepCount> attachmentReferences;
    std::array<vk::ClearValue, resourceCount> clears;
    typedef PerGpu<Util::FrameSwappedResource<vk::Framebuffer>*> fbs_t;
    fbs_t fbs;

    void makeFBs(Util::FrameSwappedResource<vk::Framebuffer>** out, size_t idx) {
      vk::RenderPass rp = renderPasses.get(idx);
      vk::Framebuffer ret[2];
      vk::ImageView views[resourceCount];//FIXME move this to a struct backing fbs and destroy them later if image destruction doesn't destroy them anyway
      Gpu& gpu = Gpu::get(idx);
      auto vkgpu = gpu.getVkDevice();
      vk::FramebufferCreateInfo fbci { {}, rp, resourceCount, views, width, height, 1 };
      for(size_t i = 0;i < 2;i++) {
	for(size_t j = 0;j < resourceCount;j++) {
	  static_cast<ImageBase*>(resources[j]->swapper.all()[i])->getVkImageView(&views[j], idx);
	}
	VK_ASSERT(vkgpu.createFramebuffer(&fbci, ALLOCCB, &ret[i]), "Failed to create framebuffer");
      }
      new(*out)Util::FrameSwappedResource<vk::Framebuffer>(ret);
    };

    void destroyFBs(Util::FrameSwappedResource<vk::Framebuffer>** f, size_t idx) {
      Gpu& gpu = Gpu::get(idx);
      auto vkgpu = gpu.getVkDevice();
      auto fbs = (*f)->all();
      for(size_t i = 0;i < fbs.size();i++)
	vkgpu.destroyFramebuffer(fbs[i], ALLOCCB);
      delete *f;
    };

    void renderQueued(WorkBatch cmd) override {
      size_t cnt = 0;
      ImageBase* images[resourceCount], *stagings[resourceCount];
      void* rams[resourceCount];
      size_t gpu = cmd.getGpuIdx();
      for(size_t i = 0;i < resourceCount;i++) {
	if(REZ[i].externalUsage) {
	  images[cnt] = resources[i]->swapper.getWrite();
	  stagings[cnt] = resources[i]->staging;
	  if(!resources[i]->hostRam)
	    resources[i]->hostRam = malloc(stagings[cnt]->getMemSize(gpu));
	  rams[cnt] = resources[i]->hostRam;
	  cnt++;
	}
      }
      WorkBatch::batchDistributeAcrossLdm(gpu, cnt, images, stagings, rams);
    };

    //TODO move most of this to constexpr if possible
    BackedRenderTarget() : RenderTarget(LDM),
			   width(512), height(512),
			   fbs(fbs_t::creator_t_F::make(this, &SPECULUM::makeFBs),
			       fbs_t::destroyer_t_F::make(this, &SPECULUM::destroyFBs)),
			   multiGpu(Gpu::ldmHasMultiplePhysical(LDM)) {
      for(uint32_t i = 0;i < resourceCount;i++) {
	resources[i] = resourceStorage.get(i);
	resources[i]->firstWriteStep = resources[i]->lastWriteStep = ~0;
	resources[i]->firstReadStep = resources[i]->lastReadStep = ~0;
	usage_t allUsages = 0;

	for(uint32_t j = 0;j < stepCount;j++) {
	  allUsages |= STEPS[j].resourceUsages[i];
	  auto oldWriteStep = resources[i]->lastWriteStep;
	  if(STEPS[j].resourceUsages[i] & GpuResource::MUSAGE_ANY_WRITE) {
	    if(resources[i]->lastReadStep < j)
	      depends[dependsCount++] = {
		resources[i]->lastReadStep, j,
		  stageFlagsForUsage(STEPS[resources[i]->lastReadStep].resourceUsages[i]),
		  stageFlagsForUsage(STEPS[j].resourceUsages[i]),//TODO <^ limit stage flags to stages actually present in pipe?
		  accessFlagsFromUsage(STEPS[resources[i]->lastReadStep].resourceUsages[i]),
		  accessFlagsFromUsage(STEPS[j].resourceUsages[i])
		  };
	    resources[i]->lastWriteStep = j;
	    if(resources[i]->firstWriteStep > stepCount)
	      resources[i]->firstWriteStep = j;
	  }
	  if(STEPS[j].resourceUsages[i] & GpuResource::MUSAGE_ANY_READ) {
	    if(oldWriteStep < j && oldWriteStep != resources[i]->lastReadStep)
	      depends[dependsCount++] = {
		oldWriteStep, j,
		stageFlagsForUsage(STEPS[oldWriteStep].resourceUsages[i]),
		stageFlagsForUsage(STEPS[j].resourceUsages[i]),//TODO <^ limit stage flags to stages actually present in pipe?
		accessFlagsFromUsage(STEPS[oldWriteStep].resourceUsages[i]),
		accessFlagsFromUsage(STEPS[j].resourceUsages[i])
	      };
	    resources[i]->lastReadStep = j;
	    if(resources[i]->firstReadStep > stepCount)
	      resources[i]->firstReadStep = j;
	  }

	  if(STEPS[j].resourceUsages[i] & GpuResource::USAGE_ATT_INPUT) {
	    attachmentReferences[j].inputs[attachmentReferences[j].inputCount++] = { i,
	      optimalLayoutForUsage(STEPS[j].resourceUsages[i]),
	      aspectMaskForUsage(REZ[i].imageData.usage)
	    };
	  }
	  if(STEPS[j].resourceUsages[i] & GpuResource::USAGE_ATT_OUTPUT) {
	    attachmentReferences[j].colors[attachmentReferences[j].colorCount++] = { i,
	      optimalLayoutForUsage(STEPS[j].resourceUsages[i])
	    };
	  }
	  if(STEPS[j].resourceUsages[i] & GpuResource::USAGE_ATT_DEPTH) {
	    attachmentReferences[j].depths[attachmentReferences[j].depthCount++] = { i,
	      optimalLayoutForUsage(STEPS[j].resourceUsages[i])
	    };
	  }
	  //do preserves on a later loop bc need info about future state usages
	}

	for(size_t j = resources[i]->firstReadStep <= resources[i]->firstWriteStep ? 0 : resources[i]->firstWriteStep+1;
	    j < (REZ[i].imageData.externalUsage ? stepCount :
		 std::max(resources[i]->lastReadStep, resources[i]->lastWriteStep));j++) {
	  if(!STEPS[j].resourceUsages[i])
	    attachmentReferences[j].preserves[attachmentReferences[j].preserveCount++] = i;
	}
	//list of usages that indicate desirable final layout: all forms of attachment, shaderreadonly, present, transfer (hostio or multi-device), readonly (multi-purpose?), else use 'general'.
	auto instance = resources[i]->swapper.getRead();
	bool preInit = REZ[i].imageData.externalUsage & GpuResource::USAGE_HOST_WRITE;
	attachments[i] = {{},
	  instance->format, (vk::SampleCountFlagBits)REZ[i].imageData.samples,
	  preInit ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
	  REZ[i].imageData.externalUsage ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare,
	  preInit ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
	  REZ[i].imageData.externalUsage ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare,
	  preInit ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined,
	  vk::ImageLayout::eTransferSrcOptimal
	};
	if(allUsages & GpuResource::USAGE_ATT_DEPTH)//ClearValue is a union so only set one member
	  clears[i].depthStencil.setDepth(1).setStencil(0);
	else
	  clears[i].color = { 0, 0, 0, 0 };//color is also a union for multiple types
      }
      for(size_t j = 0;j < stepCount;j++) {
	ASSERT_TRAP(attachmentReferences[j].colorCount == attachmentReferences[j].depthCount,
		    "vulkan requires a depth attachment for every color attachment");
	auto& step = STEPS[j];
	subpasses[j] = { {}, //flags
	    step.type == QueueType::eCompute ? vk::PipelineBindPoint::eCompute : vk::PipelineBindPoint::eGraphics,
	    0,//multiview view mask
	    attachmentReferences[j].inputCount,
	    attachmentReferences[j].inputs.data(),
	    NULL,//resolves NYI
	    attachmentReferences[j].colorCount,
	    attachmentReferences[j].colors.data(),
	    attachmentReferences[j].depths.data(),
	    attachmentReferences[j].preserveCount,
	    attachmentReferences[j].preserves.data()
	};
      }
      ci = { {}, resourceCount, attachments.data(), stepCount, subpasses.data(), dependsCount, depends };//NYI correlated views
    };

    void bindResourcesTo(ShaderDescriptorBase* sdb) override {
      GpuResource* resourcePackage[2*resourceCount];//TODO move this to constructor/constexpr
      for(size_t i = 0;i < resourceCount;i++)
	for(size_t f = 0;f < 2;f++)
	  resourcePackage[i * 2 + f] = resources[i].swapper.all()[f];
      sdb->bindResourcesUnchecked(resourcePackage);
    };

  public:
    const bool multiGpu;
    static SPECULUM* make() {
      return new SPECULUM();//instance owned by internal collection
    };

    template<size_t i> inline auto* getRead() {
      return &resourceStorage.template get<i>()->rez[resourceStorage.template get<i>()->swapper.getReadIdx()];
    };

    //called by owning object implementation on each frame where this target is not to be rendered to, to replace the new frame's write resources with the previous frame's data
    void cull() override {
      auto frame = Util::FrameCounter::getFrame();
      if(frame - 1 > lastRendered || frame == 0 || lastRendered == 0)
	return;
      for(auto gpuIdx : Gpu::gpusForLdm(LDM)) {
	WorkBatch cmd(gpuIdx);
	for(size_t i = 0;i < resourceCount;i++)
	  if(REZ[i].imageData.externalUsage)
	    cmd.copyImage(resources[i].staging, resources[i]->swapper.getWrite());
	cmd.submit();
      }
    };

  };

};
