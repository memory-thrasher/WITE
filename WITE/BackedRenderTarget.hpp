#include "RenderTarget.hpp"
#include "VulkanConverters.hpp"

namespace WITE::GPU {

  //TODO if there are attachments, and STEPS contains a compute step, add DS storage image access to the attachments
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
      FrameSwappedResource<GpuResource*> swapper;
      resource_bt(std::array<GpuResource*, 2>& data) : swapper(data, NULL) {};
    };
    template<size_t i> struct resource_ct : public resource_bt {
      static_assert(REZ[i].type == GpuResourceType::eImage,
		    "only images can be attached, and only attachments are provided by the RP.");
      static_assert((REZ[i].imageData.usage & GpuResource::USAGE_ATT_INPUT) != 0,
		    "NYI feedback loop, requires device feature test");
      GpuResourceFactory<REZ[i]>::type rez[2];
      //all gpu resources shall include a PerGpu and manage any scynchronization between them
      std::array<GpuResource*, 2> rezPtr { &rez[0], &rez[1] };
      resource_ct() : resource_bt(rezPtr) {};
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
    std::array<stepAttachmentReferences, stepCount> attachmentReferences;
    size_t dependsCount = 0;
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
  public:

    BackedRenderTarget() : RenderTarget(LDM),
			   width(512), height(512),
			   fbs(fbs_t::creator_t_F::make(this, &SPECULUM::makeFBs),
			       fbs_t::destroyer_t_F::make(this, &SPECULUM::destroyFBs)) {
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

	usage_t usage = REZ[i].imageData.usage & ~allUsages;//if it's meant to be used by something else, optimize for that and preserve it through other stages
	//FIXME edge case: if the attachment is intended to be used after this RP in ways that exclusively subset in-RP, this will assume it's not going to be used and not mark it for preservation. Maybe just always preserve or explicitly mark intermediates?
	for(size_t j = resources[i]->firstReadStep <= resources[i]->firstWriteStep ? 0 : resources[i]->firstWriteStep+1;
	    j < usage ? stepCount : max(resources[i]->firstReadStep, resources[i]->firstWriteStep);j++) {
	  if(!STEPS[j].resourceUsage[i])
	    attachmentReferences[j].preserves[attachmentReferences[j].preserveCount++] = i;
	}
	bool hasExternalUsage = usage;
	if(!usage) //if all usages were done as part of the render, we're guessing which layout it's already in for perforamnce
	  usage = STEPS[max(resources[i]->lastReadStep, resources[i]->lastWriteStep)].resourceUsages;
	//list of usages that indicate desirable final layout: all forms of attachment, shaderreadonly, present, transfer (hostio or multi-device), readonly (multi-purpose?), else use 'general'.
	auto instance = resources[i]->swapper.getRead();
	//best guess as to which layout will be best after we're done rendering it based on declared usages
	vk::ImageLayout finalLayout = optimalLayoutForUsage(usage);
	bool preInit = usage & GpuResource::USAGE_HOST_WRITE;
	attachments[i] { instance->format, REZ[i].samples,
	    preInit ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
	    hasExternalUsage ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare,
	    preInit ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
	    hasExternalUsage ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare,
	    preInit ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined,
	    finalLayout };
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

  };

};
