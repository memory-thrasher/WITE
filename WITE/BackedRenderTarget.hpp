#include "RenderTarget.hpp"
#include "VulkanConverters.hpp"
#include "TripletBacking.hpp"
#include "GpuResource.hpp"

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
    static constexpr bool COMPUTE_ENABLED = DATA.steps.countWhere([](RenderStep i) constexpr {
      return i.type == QueueType::eCompute;
    }) > 0;
  private:
#define ATTACHMENT_IMAGES GpuResource::MUSAGE_ANY_ATTACH, GpuResourceType::eImage
    typedef BackingTuple<DATA.resources> resources_t;
    resources_t resources;

    static constexpr size_t descriptorCount = resources_t::count(GpuResource::MUSAGE_ANY_DESCRIPTOR_SET);
    static constexpr size_t attachmentCount = resources_t::count(ATTACHMENT_IMAGES);
    static_assert(attachmentCount > 0);
    static constexpr Collections::CopyableArray<size_t, attachmentCount> attachmentIndexes =
      resources_t::template where<ATTACHMENT_IMAGES>();
    static constexpr Collections::CopyableArray<vk::ImageLayout, attachmentCount> attachmentInitialLayouts =
      [](size_t i)consteval{
	return (DATA.resources[attachmentIndexes[i]].imageData.externalUsage & GpuResource::USAGE_HOST_WRITE) ?
	  vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
      };
    static constexpr Collections::CopyableArray<vk::AttachmentDescription2, attachmentCount> attachmentDescriptionsDefault =
      [](size_t i)consteval{
	//format cannot be picked until runtime
	const ImageSlotData isd = DATA.resources[attachmentIndexes[i]].imageData;
	const bool preInit = isd.externalUsage & GpuResource::USAGE_HOST_WRITE;
	const bool extUsage = isd.externalUsage & ~GpuResource::USAGE_HOST_WRITE;
	return vk::AttachmentDescription2()
	  .setSamples((vk::SampleCountFlagBits)isd.samples)
	  .setLoadOp(preInit ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear)
	  .setStoreOp(extUsage ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare)
	  .setStencilLoadOp(preInit ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear)
	  .setStencilStoreOp(extUsage ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare)
	  .setInitialLayout(attachmentInitialLayouts[i]).setFinalLayout(vk::ImageLayout::eTransferSrcOptimal);
      };

    struct SubpassDescriptionStorage {
      Collections::OversizedCopyableArray<vk::AttachmentReference2, attachmentCount> inputs, colors;
      vk::AttachmentReference2 depthStencil;
      Collections::OversizedCopyableArray<uint32_t, attachmentCount> preserves;
      vk::PipelineBindPoint bp;
      constexpr SubpassDescriptionStorage(const std::vector<vk::AttachmentReference2>& inputs,
					  const std::vector<vk::AttachmentReference2>& colors,
					  const vk::AttachmentReference2& depthStencil,
					  const std::vector<uint32_t>& preserves,
					  vk::PipelineBindPoint bp) :
	inputs(inputs), colors(colors), depthStencil(depthStencil), preserves(preserves), bp(bp) {}
      constexpr ~SubpassDescriptionStorage() = default;

      constexpr vk::SubpassDescription2 makeDescription() const {
	vk::SubpassDescription2 ret;
	ret.setPipelineBindPoint(bp)
	  //.setViewMask(???)
	  .setInputAttachmentCount(inputs.len)
	  .setPInputAttachments(inputs.ptr())
	  .setColorAttachmentCount(colors.len)
	  .setPColorAttachments(colors.ptr())
	  //NYI resolves
	  .setPDepthStencilAttachment(&depthStencil)//only one allowed
	  .setPreserveAttachmentCount(preserves.len)
	  .setPPreserveAttachments(preserves.ptr());
	return ret;
      }
    };

    static constexpr Collections::CopyableArray<SubpassDescriptionStorage, DATA.steps.len> subpassDescriptionStorage =
      [](size_t i)consteval{
	std::vector<vk::AttachmentReference2> inputs, colors;
	vk::AttachmentReference2 depthStencil = { VK_ATTACHMENT_UNUSED };
	std::vector<uint32_t> preserves;
	for(uint32_t j = 0;j < attachmentCount;j++) {
	  ImageSlotData isd = DATA.resources[attachmentIndexes[j]].imageData;
	  bool used = false;
	  if(isd.usage & GpuResource::USAGE_ATT_INPUT) {
	    used = true;
	    inputs.emplace_back(j, attachmentInitialLayouts[j], aspectMaskForUsage(isd.usage));
	  }
	  if(isd.usage & GpuResource::USAGE_ATT_OUTPUT) {
	    used = true;
	    colors.emplace_back(j, attachmentInitialLayouts[j], aspectMaskForUsage(isd.usage));
	  }
	  if(isd.usage & GpuResource::USAGE_ATT_DEPTH) {//only one of these is allowed per step
	    used = true;
	    ASSERT_CONSTEXPR(depthStencil.attachment == VK_ATTACHMENT_UNUSED);
	    depthStencil = { j, attachmentInitialLayouts[j], aspectMaskForUsage(isd.usage) };
	  }
	  if(used) continue;
	  //now decide if it needs to be preserved
	  if(isd.externalUsage & ~GpuResource::USAGE_HOST_WRITE) used = true;//used by something else
	  for(size_t k = i + 1;k < DATA.steps.len && !used;k++)
	    used |= DATA.steps[k].resourceUsages[j];//used by a future step
	  if(used)
	    preserves.push_back(j);
	}
	return SubpassDescriptionStorage { inputs, colors, depthStencil, preserves,
	  DATA.steps[i].type == QueueType::eCompute ? vk::PipelineBindPoint::eCompute : vk::PipelineBindPoint::eGraphics };
      };
    static constexpr Collections::CopyableArray<vk::SubpassDescription2, DATA.steps.len> subpassDescriptions =
      [](size_t i){ return subpassDescriptionStorage[i].makeDescription(); };

    struct fb_t {
      vk::Framebuffer fb;
      vk::ImageView views[attachmentCount];
    };
    typedef PerGpu<Util::FrameSwappedResource<fb_t>*> fbs_t;
    fbs_t fbs;
    Util::FrameSwappedResource<Collections::CopyableArray<ImageBase*, attachmentCount>> attachments =
      resources.template createIndex<ImageBase*, ATTACHMENT_IMAGES>();
    Collections::CopyableArray<ImageBase*, attachmentCount> attachmentStagings =
      resources.template indexStagings<ImageBase*, ATTACHMENT_IMAGES>();
    Collections::CopyableArray<void*, attachmentCount> attachmentHostRams =
      resources.template indexHostRams<ATTACHMENT_IMAGES>();
    Collections::CopyableArray<vk::AttachmentDescription2, attachmentCount> attachmentDescriptions =
      attachmentDescriptionsDefault;
    Util::FrameSwappedResource<GpuResource*[descriptorCount]> descriptorSets =
      resources.template createIndex<GpuResource*, GpuResource::MUSAGE_ANY_DESCRIPTOR_SET>();

    void makeFBs(Util::FrameSwappedResource<fb_t>** out, size_t idx) {
      vk::RenderPass rp = renderPasses.get(idx);
      fb_t ret[2];
      {
	vk::ImageView* viewsProxy[2] = { ret[0].views, ret[1].views };
	resources.template makeImageViews<GpuResource::MUSAGE_ANY_ATTACH>(idx, viewsProxy);//populates ret[*].views
      }
      Gpu& gpu = Gpu::get(idx);
      auto vkgpu = gpu.getVkDevice();
      //pick any size bc they must all be the same anyway
      auto size = resources.template get<attachmentIndexes[0]>()->data.swapper.getWrite().getVkSize();
      vk::FramebufferCreateInfo fbci { {}, rp, attachmentCount, NULL, size.width, size.height, 1 };
      for(size_t i = 0;i < 2;i++) {
	fbci.setPAttachments(ret[i].views);
	VK_ASSERT(vkgpu.createFramebuffer(&fbci, ALLOCCB, &ret[i].fb), "Failed to create framebuffer");
      }
      *out = new Util::FrameSwappedResource<fb_t>(ret);
    };

    void destroyFBs(Util::FrameSwappedResource<fb_t>** f, size_t idx) {
      Gpu& gpu = Gpu::get(idx);
      auto vkgpu = gpu.getVkDevice();
      auto fbs = (*f)->all();
      for(size_t i = 0;i < 2;i++) {
	for(auto vw : fbs[i].views)
	  vkgpu.destroyImageView(vw, ALLOCCB);
	vkgpu.destroyFramebuffer(fbs[i].fb, ALLOCCB);
      }
      delete *f;
    };

  protected:
    virtual const Collections::IteratorWrapper<const RenderStep*> getRenderSteps() const override { return { DATA.steps }; };
    virtual void getRenderInfo(WorkBatch::renderInfo* out, size_t gpuIdx) override {
      out->rpbi = vk::RenderPassBeginInfo{
	renderPasses.get(gpuIdx),
	fbs.get(gpuIdx)->getWrite().fb,
	{ {}, resources.template get<attachmentIndexes[0]>()->data.swapper.getWrite().getVkSize() },
	attachmentCount,
	clears.ptr()
      };
      out->resourceCount = attachmentCount;
      static_assert(attachmentCount <= MAX_RESOURCES);
      memcpy(out->resources, attachments.getRead().ptr());
      memcpy(out->initialLayouts, attachmentInitialLayouts.ptr());
      memset<attachmentCount>(out->finalLayouts, vk::ImageLayout::eTransferSrcOptimal);
      out->sc = vk::SubpassContents::eInline;
    };

    void renderQueued(WorkBatch cmd) override {
      size_t gpu = cmd.getGpuIdx();
      WorkBatch::batchDistributeAcrossLdm(gpu, attachmentCount, attachments.getWrite().ptr(), attachmentStagings.ptr(), attachmentHostRams.ptr());
    };

    static constexpr Collections::CopyableArray<vk::ClearValue, attachmentCount> defaultClears =
      [](size_t i)constexpr{
	return (DATA.resources[attachmentIndexes[i]].getUsage() & GpuResource::USAGE_ATT_DEPTH) ?
	  vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)) :
	  vk::ClearValue(vk::ClearColorValue(0, 0, 0, 0));
      };

  public:
    Collections::CopyableArray<vk::ClearValue, attachmentCount> clears = defaultClears;

    BackedRenderTarget() : RenderTarget(LDM),
			   fbs(fbs_t::creator_t_F::make(this, &SPECULUM::makeFBs),
			       fbs_t::destroyer_t_F::make(this, &SPECULUM::destroyFBs))
    {
      //ci exists in parent class, parent creates render passes on demand from it.
      ci.setAttachmentCount(attachmentCount).setPAttachments(attachmentDescriptions.ptr())
	.setSubpassCount(DATA.steps.len).setPSubpasses(subpassDescriptions.ptr());
      //NOTE: not providing any subpass dependency information because the entire render pass is already strongly-ordered at the commandbuffer layer elsewhere. It may eventually be necessary. Note that the dependency can contain a MemoryBarrier on it's pNext chain to describe a layout conversion, not sure if that's necessary.
    };

    void bindResourcesTo(ShaderDescriptorBase* sdb) override {
      if constexpr(descriptorCount > 0)
	sdb->bindResourcesUnchecked(descriptorSets.all().ptr());
    };

    template<size_t i> inline auto* getRead() {
      return &resources.template get<i>()->data.swapper.getRead();
    };

    //called by owning object implementation on each frame where this target is not to be rendered to, to replace the new frame's write resources with the previous frame's data
    void cull() override {
      for(auto gpuIdx : Gpu::gpusForLdm(LDM)) {
	WorkBatch cmd(gpuIdx);
	for(size_t i = 0;i < attachmentCount;i++)
	  cmd.copyImage(attachmentStagings[i], attachments.getWrite()[i]);
	cmd.submit();
      }
    };

  };

};
