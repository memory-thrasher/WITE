#include "RenderTarget.hpp"
#include "VulkanConverters.hpp"

namespace WITE::GPU {

  struct BackedRenderTargetData {
    static constexpr size_t MAX_RESOURCES = 16, MAX_STEPS = 64;
    size_t resourceCount, stepCount;
    GpuResourceSlotInfo resources[MAX_RESOURCES];
    RenderStep steps[MAX_STEPS];
    ~BackedRenderTargetData() = default;
    BackedRenderTargetData(std::initializer_list<GpuResourceSlotInfo> rez, std::initializer_list<RenderStep> steps) :
      resourceCount(countIL(rez)), stepCount(countIL(steps)), resources(rez), steps(steps) {};
  };

  //TODO if there are attachments, and STEPS contains a compute step, add DS storage image access to the attachments
  template<BackedRenderTargetData DATA, logicalDeviceMask_t LDM>
  class BackedRenderTarget : public RenderTarget {
  public:
    typedef BackedRenderTarget<DATA, LDM> SPECULUM;
    static constexpr size_t resourceCount = DATA.resourceCount;
    static constexpr size_t stepCount = DATA.stepCount;
    static constexpr std::array<GpuResourceSlotInfo, resourceCount> REZ = subArray<0, resourceCount>(DATA.resources);
    static constexpr std::array<RenderStep, stepCount> STEPS = subArray<0, stepCount>(DATA.steps);
  protected:
    virtual const Collections::IteratorWrapper<const RenderStep*>& getRenderSteps() override const { return { steps }; };
    virtual void getRenderInfo(WorkBatch::renderInfo* out, size_t gpuIdx) override const {
      out->rpbi = { rp, fbs[gpuIdx].getWrite(), { { 0, 0 }, { width, height } }, resourceCount, clears };
      out->resourceCount = resourceCount;
      static_assert(resourceCount <= WorkBatch::renderInfo::MAX_RESOURCES);
      for(size_t i = 0;i < resourceCount;i++) {
	out->images[i] = resources[i].swapper.getWrite();
	out->initialLayouts[i] = attachments[i].initialLayout;
	out->finalLayouts[i] = attachments[i].finalLayout;
      }
      out->sc = vk::SubpassContents::eInline;
    };
  private:
    uint32_t width, height;//TODO resize function (destroy FBs, keep render pass)
    //great length to avoid new-allocation of sub-resources
    struct resource_bt {
      size_t firstWriteStep, lastWriteStep, firstReadStep, lastReadStep;
      FrameSwappedResource<ImageBase*> swapper;
      ImageBase* staging;
      void* hostRam;
      resource_bt(std::array<ImageBase*, 2>& data, GpuResource* staging) : swapper(data, NULL), staging(staging) {};
      virtual ~resource_bt() { if(hostRam) free(hostRam); };
    };
    template<size_t i> struct resource_ct : public resource_bt {
      static_assert(REZ[i].type == GpuResourceType::eImage,
		    "only images can be attached, and only attachments are provided by the RP.");
      static_assert((REZ[i].imageData.usage & GpuResource::USAGE_ATT_INPUT) != 0,
		    "NYI feedback loop, requires device feature test");
      GpuResourceFactory<REZ[i].externallyStagedResourceSlot()>::type rez[2];
      GpuResourceFactory<REZ[i].stagingResourceSlot()>::type stagingRez;
      //all gpu resources shall include a PerGpu and manage any scynchronization between them
      std::array<GpuResource*, 2> rezPtr { &rez[0], &rez[1] };
      resource_ct() : resource_bt(rezPtr, &stagingRez) {};
    };
    template<size_t i> struct resource_t {
      resource_ct<i> rezContainer;
      resource_t<i-1> next;
      inline constexpr resource_bt* get(size_t j) { return i == j ? &rezContainer : next.get(j); };
    };
    template<> struct resource_t<0> {
      resource_ct<0> rezContainer;
      inline constexpr resource_bt* get(size_t j) { return &rezContainer; };
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
    typedef PerGpu<FrameSwappedResource<vk::Framebuffer>> fbs_t;
    fbs_t fbs;

    FrameSwappedResource<vk::Framebuffer> makeFBs(size_t idx, vk::RenderPass rp) {
      vk::Framebuffer ret[2];
      vk::ImageView views[resourceCount];//FIXME move this to a struct backing fbs and destroy them later if image destruction doesn't destroy them anyway
      Gpu& gpu = get(idx);
      auto vkgpu = gpu.GetVkDevice();
      vk::FramebufferCreateInfo fbci { {}, rp, resourceCount, views, width, height, 1 };
      for(size_t i = 0;i < 2;i++) {
	for(size_t j = 0;j < resourceCount;j++) {
	  static_cast<ImageBase*>(resourcees[j]->swapper.all()[i])->makeImageView(&views[j], idx);
	}
	VK_ASSERT(vkgpu.createFramebuffer(&fbci, &ret[i]), "Failed to create framebuffer");
      }
      return { &ret };
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
	    resources[i]->hostRam = malloc(stagings[cnt]->getMemSize(gpu);
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
      for(size_t i = 0;i < resourceCount;i++) {
	resources[i] = resourceStorage.get(i);
	resources[i]->firstWriteStep = resources[i]->lastWriteStep = ~0;
	resources[i]->firstReadStep = resources[i]->lastReadStep = ~0;
	usage_t allUsages = 0;

	for(size_t j = 0;j < stepCount;j++) {
	  allUsages |= STEPS[j].resourceUsages[i];
	  auto oldWriteStep = resources[i]->lastWriteStep;
	  if(STEPS[j].resourceUsages[i] & GpuResource::MUSAGE_ANY_WRITE) {
	    if(resources[i]->lastReadStep < j)
	      depends[dependsCount++] { resources[i]->lastReadStep, j,
		  stageFlagsForUsage(STEPS[resources[i]->lastReadStep].resourceUsages[i]),
		  stageFlagsForUsage(STEPS[j].resourceUsages[i]),//TODO <^ limit stage flags to stages actually present in pipe?
		  STEPS[resources[i]->lastReadStep].resourceUsages[i],
		  STEPS[j].resourceUsages[i]
		  };
	    resources[i]->lastWriteStep = j;
	    if(resources[i]->firstWriteStep > stepCount)
	      resources[i]->firstWriteStep = j;
	  }
	  if(STEPS[j].resourceUsages[i] & GpuResource::MUSAGE_ANY_READ) {
	    if(oldWriteStep < j && oldWriteStep != resources[i]->lastReadStep)
	      depends[dependsCount++] { oldWriteStep, j,
		  stageFlagsForUsage(STEPS[oldWriteStep].resourceUsages[i]),
		  stageFlagsForUsage(STEPS[j].resourceUsages[i]),//TODO <^ limit stage flags to stages actually present in pipe?
		  STEPS[oldWriteStep].resourceUsages[i],
		  STEPS[j].resourceUsages[i]
		  };
	    resources[i]->lastReadStep = j;
	    if(resources[i]->firstReadStep > stepCount)
	      resources[i]->firstReadStep = j;
	  }

	  bool used = false;
	  if(STEPS[j].resourceUsage[i] & GpuResource::USAGE_ATT_INPUT) {
	    used = true;
	    attachmentReferences[j].inputs[attachmentReferences[j].inputCount++] = { i,
	      optimalLayoutForUsage(STEP[j].resourceUsage[i]),
	      aspectMaskForUsage(REZ[i].imageData.usage)
	    };
	  }
	  if(STEPS[j].resourceUsage[i] & GpuResource::USAGE_ATT_COLOR) {
	    used = true;
	    attachmentReferences[j].colors[attachmentReferences[j].colorCount++] = { i,
	      optimalLayoutForUsage(STEP[j].resourceUsage[i])
	    };
	  }
	  if(STEPS[j].resourceUsage[i] & GpuResource::USAGE_ATT_DEPTH) {
	    used = true;
	    attachmentReferences[j].depths[attachmentReferences[j].depthCount++] = { i,
	      optimalLayoutForUsage(STEP[j].resourceUsage[i])
	    };
	  }
	  //do preserves on a later loop bc need info about future state usages
	}

	for(size_t j = resources[i]->firstReadStep <= resources[i]->firstWriteStep ? 0 : resources[i]->firstWriteStep+1;
	    j < (REZ[i].externalUsage ? stepCount : max(resources[i]->lastReadStep, resources[i]->lastWriteStep));j++) {
	  if(!STEPS[j].resourceUsage[i])
	    attachmentReferences[j].preserves[attachmentReferences[j].preserveCount++] = i;
	}
	//list of usages that indicate desirable final layout: all forms of attachment, shaderreadonly, present, transfer (hostio or multi-device), readonly (multi-purpose?), else use 'general'.
	auto instance = resources[i]->swapper.getRead();
	bool preInit = REZ[i].externalUsage & GpuResource::USAGE_HOST_WRITE;
	attachments[i] = { instance->format, REZ[i].samples,
	  preInit ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
	  REZ[i].externalUsage ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare,
	  preInit ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
	  REZ[i].externalUsage ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare,
	  preInit ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined,
	  vk::ImageLayout::eTransferSrc
	};
	if(allUsages[i] & GpuResource::USAGE_ATT_DEPTH)//ClearValue is a union so only set one member
	  clears[i].depthStencil { 1.0, 0 };
	else
	  clears[i].color.int32 { 0, 0, 0, 0 };//color is also a union for multiple types
      }
      for(size_t j = 0;j < stepCount;j++) {
	ASSERT_TRAP(attachmentReferences[j].colorCount == attachmentReferences[j].depthCount,
		    "vulkan requires a depth attachment for every color attachment");
	auto& step = STEPS[j];
	subpasses[j] { {}, //flags
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

    //called by owning object implementation on each frame where this target is not to be rendered to, to replace the new frame's write resources with the previous frame's data
    void cull() override {
      auto frame = Util::FrameCounter::getFrame();
      if(frame - 1 > lastRendered || frame == 0 || lastRendered == 0)
	return;
      for(auto gpuIdx : Gpu::gpusForLdm(LDM)) {
	WorkBatch cmd(gpuIdx);
	for(size_t i = 0;i < resourceCount;i++)
	  if(REZ[i].externalUsage)
	    cmd.copyImage(resources[i].staging, resources[i]->swapper.getWrite());
	cmd.submit();
      }
    };

  };

};
