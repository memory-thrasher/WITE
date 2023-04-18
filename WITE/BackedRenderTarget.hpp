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
    static constexpr size_t attachmentIndexes[attachmentCount] = resources_t::template where<ATTACHMENT_IMAGES>();
    static constexpr vk::ImageLayout attachmentInitialLayouts[attachmentCount] =
      populateFor<attachmentCount>([](size_t i)constexpr{
	return (DATA.resources[attachmentIndexes[i]].imageData.externalUsage & GpuResource::USAGE_HOST_WRITE) ?
	  vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
      });
    static constexpr vk::AttachmentDescription2 attachmentDescriptionsDefault[attachmentCount] =
      populateFor<attachmentCount>([](size_t i)constexpr{
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
      });
    static constexpr vk::SubpassDescription2 subpassDescriptions[DATA.steps.len] =
      populateFor<DATA.steps.len>([](size_t i)constexpr{
	std::vector<vk::AttachmentReference2> inputs, colors, depthStencils;
	std::vector<uint32_t> preserves;
	for(size_t j = 0;j < attachmentCount;j++) {
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
	  if(isd.usage & GpuResource::USAGE_ATT_DEPTH) {
	    used = true;
	    depthStencils.emplace_back(j, attachmentInitialLayouts[j], aspectMaskForUsage(isd.usage));
	  }
	  if(used) continue;
	  //now decide if it needs to be preserved
	  if(isd.externalUsage & !GpuResource::USAGE_HOST_WRITE) used = true;//used by something else
	  for(size_t k = i + 1;k < DATA.steps.len && !used;k++)
	    used |= DATA.steps[k].resourceUsages[j];//used by a future step
	  if(used)
	    preserves.push_back(j);
	}
	const RenderStep step = DATA.steps[i];
	ASSERT_CONSTEXPR(depthStencils.size() <= 1);
	if(depthStencils.size() == 0)
	  depthStencils.emplace_back(VK_ATTACHMENT_UNUSED);
	return vk::SubpassDescription2().setPipelineBindPoint(step.type == QueueType::eCompute ?
							      vk::PipelineBindPoint::eCompute : vk::PipelineBindPoint::eGraphics)
	  //.setViewMask(???)
	  .setInputAttachmentCount(inputs.size())
	  .setPInputAttachments(inputs.data())
	  .setColorAttachmentCount(colors.size())
	  .setPColorAttachments(colors.data())
	  //NYI resolves
	  .setPDepthStencilAttachment(depthStencils.data())//only one allowed
	  .setPreserveAttachmentCount(preserves.size())
	  .setPPreserveAttachments(preserves.data());
      });

    struct fb_t {
      vk::Framebuffer fb;
      vk::ImageView views[attachmentCount];
    };
    typedef PerGpu<Util::FrameSwappedResource<fb_t>*> fbs_t;
    fbs_t fbs;
    Util::FrameSwappedResource<ImageBase*[attachmentCount]> attachments =
      resources.template createIndex<ImageBase*, ATTACHMENT_IMAGES>();
    ImageBase* attachmentStagings[attachmentCount] = resources.template indexStagings<ImageBase*, ATTACHMENT_IMAGES>();
    void* attachmentHostRams[attachmentCount] = resources.template indexHostRams<ATTACHMENT_IMAGES>();
    vk::AttachmentDescription2 attachmentDescriptions[attachmentCount] = attachmentDescriptionsDefault;
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
	VK_ASSERT(vkgpu.createFramebuffer(&fbci, ALLOCCB, &ret[i]), "Failed to create framebuffer");
      }
      *out = new Util::FrameSwappedResource<fb_t>(ret);
    };

    void destroyFBs(Util::FrameSwappedResource<fb_t>** f, size_t idx) {
      Gpu& gpu = Gpu::get(idx);
      auto vkgpu = gpu.getVkDevice();
      auto fbs = (*f)->all();
      for(size_t i = 0;i < 2;i++) {
	for(auto vw : fbs[i].views)
	  vkgpu.destroyImageView(*vw, ALLOCCB);
	vkgpu.destroyFramebuffer(fbs[i].fb, ALLOCCB);
      }
      delete *f;
    };

  protected:
    virtual const Collections::IteratorWrapper<const RenderStep*> getRenderSteps() const override { return { DATA.steps }; };
    virtual void getRenderInfo(WorkBatch::renderInfo* out, size_t gpuIdx) override {
      out->rpbi = { renderPasses.get(gpuIdx), fbs.get(gpuIdx)->getWrite().fb,
	resources.template get<attachmentIndexes[0]>()->data.swapper.getWrite().getAllInclusiveSubresource(),
	attachmentCount, clears };
      out->resourceCount = attachmentCount;
      static_assert(attachmentCount <= MAX_RESOURCES);
      memcpy(out->resources, attachments.getRead());
      memcpy(out->initialLayouts, attachmentInitialLayouts);
      memset<attachmentCount>(out->finalLayouts, vk::ImageLayout::eTransferSrcOptimal);
      out->sc = vk::SubpassContents::eInline;
    };

    void renderQueued(WorkBatch cmd) override {
      size_t gpu = cmd.getGpuIdx();
      WorkBatch::batchDistributeAcrossLdm(gpu, attachmentCount, attachments.getWrite(), attachmentStagings, attachmentHostRams);
    };

    static constexpr vk::ClearValue defaultClears[attachmentCount] =
      populateFor<attachmentCount, vk::ClearValue>([](size_t i)constexpr{
	return (DATA.resources[attachmentIndexes[i]].getUsage() & GpuResource::USAGE_ATT_DEPTH) ?
	  vk::ClearDepthStencilValue(1.0f, 0) : vk::ClearColorValue(0, 0, 0, 0);
      });

  public:
    vk::ClearValue clears[attachmentCount] = defaultClears;

    BackedRenderTarget() : fbs(fbs_t::creator_t_F::make(this, &SPECULUM::makeFBs),
			       fbs_t::destroyer_t_F::make(this, &SPECULUM::destroyFBs))
    {
      //ci exists in parent class, parent creates render passes on demand from it.
      ci.setAttachmentCount(attachmentCount).setPAttachments(attachmentDescriptions)
	.setSubpassCount(DATA.steps.len).setPSubpasses(subpassDescriptions);
      //NOTE: not providing any subpass dependency information because the entire render pass is already strongly-ordered at the commandbuffer layer elsewhere. It may eventually be necessary. Note that the dependency can contain a MemoryBarrier on it's pNext chain to describe a layout conversion, not sure if that's necessary.
    };

    void bindResourcesTo(ShaderDescriptorBase* sdb) override {
      sdb->bindResourcesUnchecked(descriptorSets.all());
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
