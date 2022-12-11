#pragma once

#include "GpuResource.hpp"
#include "Vulkan.hpp"
#include "PerGpuPerThread.hpp"
#include "PerGpu.hpp"
#include "VRam.hpp"

namespace WITE::GPU {

  class Gpu;

  class ImageBase : public GpuResource {
  private:
    typedef Collections::PerGpuPerThread<vk::ImageView> views_t;
    views_t views;
    PerGpu<vk::Image> images;
    uint32_t w, h, z;

    void makeImage(vk::Image*, size_t gpu);
    static void destroyImage(vk::Image*, size_t gpu);
  public:
    const ImageSlotData slotData;
    const vk::Format format;
    const bool transfersRequired;//if this image is to be transferred from it's device: ie if it's used by multiple phys devs

    virtual ~ImageBase() = default;
    ImageBase(ImageBase&) = delete;
    ImageBase(ImageSlotData isd);
    void getCreateInfo(Gpu&, vk::ImageCreateInfo* out, size_t width, size_t height, size_t z = 1);
    vk::ImageUsageFlags getVkUsageFlags();
    vk::AttachmentDescription getAttachmentDescription(bool clear = true);
    virtual vk::Image getVkImage(size_t gpuIdx) = 0;
    vk::ImageView getVkImageView(size_t gpuIdx);
    void makeImageView(vk::ImageView*, size_t gpu);
    static void destroyImageView(vk::ImageView& doomed, size_t gpu);
    void resize(size_t w, size_t h, size_t z);//TODO for each existing image, blit it to the new one. Crash if not transfer-enabled (maybe make a resizable usage flag?).
    inline vk::Extent2D getVkSize() { return { w, h }; };
    vk::ImageSubresourceRange getAllInclusiveSubresource();
  };

  //MIP and SAM might be less than requested if the platform does not support it
  template<ImageSlotData ISD> class Image : ImageBase {
  public:
    Image() : ImageBase(ISD) {};
    ~Image() override = default;
  };

  template<ImageSlotData ISD> struct GpuResourceFactory<ISD> {
    typedef Image<ISD> type;
    auto operator()() { return Image<ISD>(); };
  };

}
