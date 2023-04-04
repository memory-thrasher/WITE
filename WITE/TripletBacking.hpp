#include "GpuResource.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

namespace WITE::GPU {

  template<GpuResourceSlotInfo SLOT> auto makeResource() {
    if constexpr(SLOT.type == GpuResourceType::eImage)
      return Image<SLOT.imageData>();
    else if constexpr(SLOT.type == GpuResourceType::eBuffer)
      return Buffer<SLOT.bufferData>();
  };

  template<GpuResourceSlotInfo SLOT> using gpuResource_t = std::invoke_result<makeResource>::type;

  template<GpuResourceSlotInfo SLOT> struct TripletBacking {
    auto staging = makeResource<SLOT.stagingResourceSlot()>();
    typedef decltype(staging) staging_t;
    typedef gpuResource_t<SLOT.externallyStagedResourceSlot()> T;
    Util::FrameSwappedResource<T> swapper;
    std::unique_ptr<void> hostRam;
  };

  template<Collections::LiteralList<GpuResourceSlotInfo> SLOTS, size_t L = SLOTS.len> struct BackingTuple {
    TripletBacking<SLOTS[0]> data;
    BackingTuple<SLOTS.sub(1, SLOTS.len - 1)> rest;

    template<size_t I> inline auto* get() { if constexpr(I == 0) return this; return rest.get<I-1>(); };

    constexpr static inline size_t count(usage_t usage, GpuResourceType type) {
      return SLOTS.countWhere([usage, type](GpuResourceSlot& s) {
	return (s.getUsage() & usage) == usage && type == s.type;
      });
    };

    template<usage_t usage, size_t imageCount = count(usage, GpuResourceType::eImage)>
    inline vk::ImageView[2*imageCount] makeImageViews(size_t gpuIdx) {
      vk::ImageView ret[2*imageCount];
      vk::ImageView* proxy[2] = { &ret[0], &ret[imageCount] };
      makeImageViews<usage>(gpuIdx, proxy);
      return ret;
    };

    template<usage_t usage> inline void makeImageViews(size_t gpuIdx, vk::ImageView* (&out)[2]) {
      if constexpr(SLOTS[0].type == GpuResourceType::eImage && (SLOTS[0].getUsage() & usage) == usage)
	for(size_t i = 0;i < 2;i++)
	  (*out[i]++) = data.swapper.all()[i].getVkImageView(gpuIdx);
      if constexpr(L > 1)
	rest.makeImageViews<usage>(gpuIdx, out);
    };

    template<usage_t usage, GpuResourceType type> inline auto& first() {
      if constexpr(SLOTS[0].type == type && (SLOTS[0].getUsage() & usage) == usage)
	return data;
      else
	return rest.first<usage, type>();
    };

  };

  template<Collections::LiteralList<GpuResourceSlotInfo> SLOTS> struct BackingTuple<SLOTS, 0> {};//empty

}
