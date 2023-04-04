#include "RenderTarget.hpp"
#include "VulkanConverters.hpp"
#include "TripletBacking.hpp"

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
    static constexpr bool COMPUTE_ENABLED = DATA.steps.countWhere([](RenderStep i) constexpr {
      return i.type == QueueType::eCompute;
    }) > 0;
  private:
    typedef BackingTuple<DATA.resources> resources_t;
    resources_t resources;
    static constexpr size_t attachmentCount = resources_t::count(GpuResource::MUSAGE_ANY_ATTACH, GpuResourceType::eImage);
    struct fb_t {
      vk::Framebuffer fb;
      vk::ImageView views[attachmentCount];
    };
    typedef PerGpu<Util::FrameSwappedResource<fb_t>*> fbs_t;
    fbs_t fbs;

    void makeFBs(Util::FrameSwappedResource<fb_t>** out, size_t idx) {
      vk::RenderPass rp = renderPasses.get(idx);
      fb_t ret[2];
      {
	vk::ImageView* viewsProxy[2] = { ret[0].views, ret[1].views };
	resources.makeImageViews<GpuResource::MUSAGE_ANY_ATTACH>(idx, viewsProxy);//populates ret[*].views
      }
      Gpu& gpu = Gpu::get(idx);
      auto vkgpu = gpu.getVkDevice();
      //pick any size bc they must all be the same anyway
      auto size = resources.first<GpuResource::MUSAGE_ANY_ATTACH, GpuResourceType::eImage>().getRead().getVkSize();
      vk::FramebufferCreateInfo fbci { {}, rp, attachmentCount, NULL, size.width, size.height, 1 };
      for(size_t i = 0;i < 2;i++) {
	fbci.setPAttachments(ret[i].views);
	VK_ASSERT(vkgpu.createFramebuffer(&fbci, ALLOCCB, &ret[i]), "Failed to create framebuffer");
      }
      *out = new Util::FrameSwappedResource<fb_t>(ret);
    };

    void destroyFBs(Util::FrameSwappedResource<FB_T>** f, size_t idx) {
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

    static constexpr size_t firstAttachmentIdx = DATA.resources.countWhere([](GpuResourceSlotInfo i) constexpr {
      return (i.getUsage() & GpuResource::MUSAGE_ANY_ATTACH) == 0;
    });

    #error TODO finish rewrite

  protected:
    virtual const Collections::IteratorWrapper<const RenderStep*> getRenderSteps() const override { return { DATA.steps }; };
    virtual void getRenderInfo(WorkBatch::renderInfo* out, size_t gpuIdx) override {
#error break TODO audit below here;   need: frameBuffers, clears
      out->rpbi = { renderPasses.get(gpuIdx), fbs.get(gpuIdx)->getWrite(),
	resources.get<firstAttachmentIdx>()->data.getAllInclusiveSubresource(),
	resourceCount, clears.data() };
      out->resourceCount = resourceCount;
      static_assert(resourceCount <= MAX_RESOURCES);
      for(size_t i = 0;i < resourceCount;i++) {
	out->resources[i] = resources[i]->swapper.getWrite();
	out->initialLayouts[i] = attachments[i].initialLayout;
	out->finalLayouts[i] = attachments[i].finalLayout;
      }
      out->sc = vk::SubpassContents::eInline;
    };

  public:
  };

};
