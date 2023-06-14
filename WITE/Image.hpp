#pragma once

#include "GpuResource.hpp"
#include "Vulkan.hpp"
#include "PerGpuPerThread.hpp"
#include "PerGpu.hpp"
#include "VRam.hpp"
#include "StateSynchronizer.hpp"

namespace WITE::GPU {

  class Gpu;
  class WorkBatch;

  class ImageBase : public GpuResource {
  private:
    struct accessState;
    struct accessObject {
      vk::ImageView view;
      vk::Sampler sampler;
      vk::DescriptorImageInfo dsImageInfo;
    };
    typedef Collections::PerGpuPerThread<std::unique_ptr<accessObject>> accessors_t;
    accessors_t accessors;
    PerGpu<vk::Image> images;
    PerGpu<VRam> mem;
    uint32_t w, h, z;

    void makeImage(vk::Image*, size_t gpu);
    static void destroyImage(vk::Image*, size_t gpu);
    void makeStateTracker(StateSynchronizer<accessState>* ret, size_t gpu);

    struct accessState {
      vk::ImageLayout layout;
      vk::AccessFlags2 accessMask;
      vk::PipelineStageFlags2 stageMask;
      uint32_t queueFam;
      auto operator<=>(const accessState& o) const = default;
    };

    PerGpu<StateSynchronizer<accessState>> accessStateTracker;
    void updateAccessState(size_t gpuIdx, accessState, accessState*, WorkBatch&);

    friend class WorkBatch;
  public:
    const ImageSlotData slotData;
    const vk::Format format;
    const bool transfersRequired;//if this image is to be transferred from it's device: ie if it's used by multiple phys devs

    static inline auto getBestImageFormat(const ImageSlotData isd) {
      return Gpu::getBestImageFormat(isd.components, isd.componentsSize, isd.usage | isd.externalUsage,
				     isd.logicalDeviceMask, isd.shouldBeLinear());
    };

    virtual ~ImageBase() = default;
    ImageBase(const ImageBase&);
    ImageBase(ImageSlotData isd, GpuResourceInitData grid);
    void getCreateInfo(Gpu&, vk::ImageCreateInfo* out, size_t width, size_t height, size_t z = 1);
    vk::MemoryPropertyFlags getMemFlags();
    vk::ImageUsageFlags getVkUsageFlags();
    vk::AttachmentDescription getAttachmentDescription(bool clear = true);
    vk::ImageSubresourceLayers getAllSubresourceLayers();
    vk::Image getVkImage(size_t gpuIdx);
    vk::ImageView makeVkImageView(size_t gpuIdx);
    vk::ImageView getVkImageView(size_t gpuIdx);
    void makeAccessors(accessObject*, size_t gpu);
    static void destroyAccessors(accessObject* doomed, size_t gpu);
    void resize(size_t w, size_t h, size_t z);//TODO for each existing image, blit it to the new one. Crash if not transfer-enabled (maybe make a resizable usage flag?).
    inline vk::Extent2D getVkSize() { return { w, h }; };
    inline vk::Extent3D getVkSize3D() { return { w, h, z }; };
    vk::ImageAspectFlags getAspects();
    vk::ImageSubresourceRange getAllInclusiveSubresource();

    void populateDSWrite(vk::WriteDescriptorSet* out, size_t gpuIdx) override;
    void ensureExists(size_t gpu);
    size_t getMemSize(size_t gpu);
    size_t getMaxMemSize();
    inline void read(void* data, size_t len, size_t gpu) { mem.getRef(gpu).read(data, len); };
    inline void write(void* data, size_t len, size_t gpu) { mem.getRef(gpu).write(data, len); };
  };

  //MIP and SAM might be less than requested if the platform does not support it
  template<ImageSlotData ISD> class Image : public ImageBase {
  public:
    static constexpr ImageSlotData SLOT_DATA = ISD;
    Image(const GpuResourceInitData grid) : ImageBase(ISD, grid) {};
    Image(const Image<ISD>& i) : ImageBase(i) {};
    ~Image() override = default;
  };

}
