#include "RenderTarget.hpp"
#include "VulkanConverters.hpp"

namespace WITE::GPU {

  //TODO if there are attachments, and STEPS contains a compute step, add DS storage image access to the attachments
  #error TODO Make RT only creatable via factory and destroy (flag for later destruction) with dispose function
  template<StructuralConstList<GpuResourceSlotInfo> REZ, StructuralConstList<RenderStep> STEPS, logicalDeviceMask_t LDM>
  class BackedRenderTarget : public RenderTarget {
  public:
    typedef BackedRenderTarget<REZ, STEPS, LDM> SPECULUM;
    static constexpr size_t resourceCount = REZ.count();
    static constexpr size_t stepCount = STEPS.count();
    static constexpr StructuralConstList<GpuResourceSlotInfo> resourceSlots;
    static constexpr StructuralConstList<RenderStep> steps = STEPS;
  protected:
    virtual const StructuralConstList<RenderStep>& getRenderSteps() override const { return steps; };
    virtual void getRenderInfo(vk::RenderPassBeginInfo* out, size_t gpuIdx) override const {
      *out = { rp, fbs[gpuIdx].getWrite(), { { 0, 0 }, { width, height } }, resourceCount, clears };
    };
  private:
    uint32_t width, height;//TODO resize function (destroy FBs, keep render pass)
    //great length to avoid new-allocation of sub-resources
    struct resource_bt {
      size_t firstWriteStep, lastWriteStep, firstReadStep, lastReadStep;
      FrameSwappedResource<ImageBase*> swapper;
      ImageBase* staging;
      resource_bt(std::array<ImageBase*, 2>& data, GpuResource* staging) : swapper(data, NULL), staging(staging) {};
    };
    template<size_t i> struct resource_ct : public resource_bt {
      static_assert(REZ[i].type == GpuResourceType::eImage,
		    "only images can be attached, and only attachments are provided by the RP.");
      static_assert((REZ[i].imageData.usage & GpuResource::USAGE_ATT_INPUT) != 0,
		    "NYI feedback loop, requires device feature test");
      GpuResourceFactory<REZ[i].externallyStagedResourceSlot()>::type rez[2];
      GpuResourceFactory<RES[i].stagingResourceSlot()>::type stagingRez;
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
    resource_t<REZ.count()-1> resourceStorage;
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
      vk::ImageView views[REZ.count()];//FIXME move this to a struct backing fbs and destroy them later if image destruction doesn't destroy them anyway
      Gpu& gpu = get(idx);
      auto vkgpu = gpu.GetVkDevice();
      vk::FramebufferCreateInfo fbci { {}, rp, REZ.count(), views, width, height, 1 };
      for(size_t i = 0;i < 2;i++) {
	for(size_t j = 0;j < REZ.count();j++) {
	  static_cast<ImageBase*>(resourcees[j]->swapper.all()[i])->makeImageView(&views[j], idx);
	}
	VK_ASSERT(vkgpu.createFramebuffer(&fbci, &ret[i]), "Failed to create framebuffer");
      }
      return { &ret };
    };

    void renderQueued(ElasticCommandBuffer& cmd) override {
      ElasticCommandBuffer* transferCmd = NULL;
      if(!cmd.supportsTransfer() && Gpu::ldmHasMultiplePhysical(LDM)) {
	transferCmd = &cmd.getTransfer();
      } else {
	transferCmd = &cmd;
      }
      size_t gpuIdx = cmd->getGpu()->getIndex();
      size_t mbCnt = 0;
      vk::ImageMemoryBarrier mbs[resourceCount*2];
      std::bitset<resourceCount> transferMask[resourceCount];
      for(size_t i = 0;i < resourceCount;i++) {
	resource_bt* r = resources[i];
	if(r->staging.transfersRequired && REZ[i].externalUsage) {
	  transferMask[i] = true;
	  //NOTE: render pass has already transitioned the rendered image to transfer src optimal
	  mbs[mbCnt++] = { //memory barrier: discard staging image into transfer dst
	    vk::PipelineStageFlagBits::eNone, vk::AccessFlagBits::eNone,
	    vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite,
	    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
	    cmd.getQueue()->family, transferCmd->getQueue()->family,
	    r->staging->getVkImage(gpuIdx),
	    r->staging->getAllInclusiveSubresource()
	  };
	}
      }
      vk::DependencyInfo di { {}, 0, NULL, 0, NULL, mbCnt, mbs };
      cmd->pipelineBarrier2(&di);
      for(size_t i = 0;i < resourceCount;i++) {
	if(!transferMask[i]) continue;
	resource_bt* r = resources[i];
	vk::ImageCopy imageCopy {
	  r->swapper->getWrite().getAllInclusiveSubResource(), {},
	  r->staging->getAllInclusiveSubResource(), {}, { width, height, REZ[i].imageData.arrayLayers }
	};
	transferCmd->copyImage(r->swapper->getWrite().getVkImage(gpuIdx), vk::ImageLayout::eTransferSrcOptimal,
			       r->staging->getVkImage(gpuIdx), vk::ImageLayout::eTransferDstOptimal,
			       1, &imageCopy);
      }
      mbCnt = 0;
      for(size_t i = 0;i < resourceCount;i++) {
	if(!transferMask[i]) continue;
	mbs[mbCnt++] = { //memory barrier: image to external usage optimal
	  vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferRead,
	  vk::PipelineStageFlagBits::eAllCommands, vk::AccessFlagBits::eShaderRead,
	  vk::ImageLayout::eTransferSrcOptimal, optimalLayoutForUsage(REZ[i].externalUsage),
	  transferCmd->getQueue()->family, cmd.getQueue()->family,
	  r->swapper->getWrite().getVkImage(gpuIdx),
	  r->swapper->getWrite().getAllInclusiveSubresource()
	};
	mbs[mbCnt++] = {//memory barrier (concurrent): staging to host read optimal
	  vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite,
	  vk::PipelineStageFlagBits::eHost, vk::AccessFlagBits::eHostRead,
	  vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
	  transferCmd->getQueue()->family, cmd.getQueue()->family, //assumption: cmd = graphics queue
	  r->staging->getVkImage(gpuIdx),
	  r->staging->getAllInclusiveSubresource()
	};
      }
      di.imageMemoryBarrierCount = mbCnt;
      (*transferCmd)->pipelineBarrier2(&di);
      transferCmd->addSignal(sem[gpuIdx]);
      transferCmd->addSignal(truckSem[gpuIdx]);
    };

    virtual void truckResourcesTo(size_t gpuSrc, size_t gpuDst, ElasticCommandBuffer& cmd) {
      for(resource_bt* r : resources)
	r->staging->copy(gpuSrc, gpuDst, cmd);
    }

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
	//best guess as to which layout will be best after we're done rendering it based on declared usages
	vk::ImageLayout finalLayout = multiGpu ? vk::ImageLayout::eTransferSrc : optimalLayoutForUsage(REZ[i].externalUsage);
	bool preInit = REZ[i].externalUsage & GpuResource::USAGE_HOST_WRITE;
	attachments[i] = { instance->format, REZ[i].samples,
	  preInit ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
	  REZ[i].externalUsage ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare,
	  preInit ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
	  REZ[i].externalUsage ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare,
	  preInit ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined,
	  finalLayout
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

  public:
    const bool multiGpu;
    static SPECULUM* make() {
      return new SPECULUM();//instance owned by internal collection
    };

  };

};
