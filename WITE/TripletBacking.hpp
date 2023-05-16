#include "GpuResource.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

namespace WITE::GPU {

  template<GpuResourceSlotInfo SLOT> struct resourceFactory {
    auto operator()() {
      if constexpr(SLOT.type == GpuResourceType::eImage)
	return Image<SLOT.imageData>();
      else if constexpr(SLOT.type == GpuResourceType::eBuffer)
	return Buffer<SLOT.bufferData>();
    };
  };

  template<GpuResourceSlotInfo SLOT> using gpuResource_t = std::invoke_result<resourceFactory<SLOT>>::type;

  template<GpuResourceSlotInfo SLOT> struct TripletBacking {
    typedef gpuResource_t<SLOT.stagingResourceSlot()> staging_t;
    staging_t staging = resourceFactory<SLOT.stagingResourceSlot()>()();
    typedef gpuResource_t<SLOT.externallyStagedResourceSlot()> T;
    Util::FrameSwappedResource<T, 2> swapper = { {
	resourceFactory<SLOT.externallyStagedResourceSlot()>()(),
	resourceFactory<SLOT.externallyStagedResourceSlot()>()()
      }, NULL };
    void* hostRam;
    ~TripletBacking() { if(hostRam) free(hostRam); };
  };

  template<Collections::LiteralList<GpuResourceSlotInfo> SLOTS, size_t L = SLOTS.len> struct BackingTuple {
    TripletBacking<SLOTS[0]> data;
    typedef BackingTuple<SLOTS.sub(1, SLOTS.len - 1)> rest_t;
    rest_t rest;

    template<size_t I> inline auto* get() { if constexpr(I == 0) return this; else return rest.template get<I-1>(); };

    consteval static inline size_t count(usage_t usage) {
      return SLOTS.countWhere([usage](auto s) {
	return s.getUsage() & usage;
      });
    };

    consteval static inline size_t count(usage_t usage, GpuResourceType type) {
      return SLOTS.countWhere([usage, type](auto& s) {
	return (s.getUsage() & usage) && type == s.type;
      });
    };

    template<usage_t usage, GpuResourceType type, size_t R = count(usage, type)>
    consteval static inline auto where() {
      Collections::CopyableArray<size_t, R> ret;
      where<usage, type, 0>(ret.ptr());
      return ret;
    };

    template<usage_t usage, GpuResourceType type, size_t I>
    consteval static inline void where(size_t* ret) {
      if constexpr(SLOTS[0].type == type && (SLOTS[0].getUsage() & usage))
	*ret++ = I;
      if constexpr(L > 1)
	rest_t::template where<usage, type, I+1>(ret);
    };

    template<class T, usage_t usage>
    auto inline createIndex() {
      constexpr static size_t LEN = count(usage);
      Collections::CopyableArray<T[LEN], 2> ret;
      createIndex<T, usage>(ret[0], ret[1]);
      return Util::FrameSwappedResource<T[LEN]>(ret.ptr(), NULL);
    };

    template<class T, usage_t usage>
    void inline createIndex(T* odd, T* even) {
      if constexpr((SLOTS[0].getUsage() & usage)) {
	auto& d = data.swapper.all();
	*odd++ = auto_cast<T>(d[0]);
	*even++ = auto_cast<T>(d[1]);
      }
      if constexpr(L > 1)
	rest.template createIndex<T, usage>(odd, even);
    };

    template<class T, usage_t usage, GpuResourceType type>
    auto inline createIndex() {
      constexpr static size_t LEN = count(usage, type);
      std::array<T[LEN], 2> ret;
      createIndex<T, usage, type>(ret[0], ret[1]);
      return Util::FrameSwappedResource<T[LEN]>(ret, NULL);
    };

    template<class T, usage_t usage, GpuResourceType type>
    void inline createIndex(T* odd, T* even) {
      if constexpr(SLOTS[0].type == type && (SLOTS[0].getUsage() & usage)) {
	auto& d = data.swapper.all();
	*odd++ = auto_cast<T>(d[0]);
	*even++ = auto_cast<T>(d[1]);
      }
      if constexpr(L > 1)
	rest.template createIndex<T, usage, type>(odd, even);
    };

    template<class T, usage_t usage, GpuResourceType type>
    auto inline indexStagings() {
      constexpr static size_t LEN = count(usage, type);
      Collections::CopyableArray<T, LEN> ret;
      indexStagings<T, usage, type>(ret.ptr());
      return ret;
    };

    template<class T, usage_t usage, GpuResourceType type>
    void inline indexStagings(T* out) {
      if constexpr(SLOTS[0].type == type && (SLOTS[0].getUsage() & usage))
	*out++ = auto_cast<T>(data.staging);
      if constexpr(L > 1)
	rest.template indexStagings<T, usage, type>(out);
    };

    template<usage_t usage, GpuResourceType type>
    auto inline indexHostRams() {
      constexpr static size_t LEN = count(usage, type);
      Collections::CopyableArray<void*, LEN> ret;
      indexHostRams<usage, type>(ret.ptr());
      return ret;
    };

    template<usage_t usage, GpuResourceType type>
    void inline indexHostRams(void** out) {
      if constexpr(SLOTS[0].type == type && (SLOTS[0].getUsage() & usage))
	*out++ = data.hostRam.get();
      if constexpr(L > 1)
	rest.template indexHostRams<usage, type>(out);
    };

    template<usage_t usage, size_t imageCount = count(usage, GpuResourceType::eImage)>
    inline auto makeImageViews(size_t gpuIdx) {
      vk::ImageView ret[2*imageCount];
      vk::ImageView* proxy[2] = { &ret[0], &ret[imageCount] };
      makeImageViews<usage>(gpuIdx, proxy);
      return ret;
    };

    template<usage_t usage> inline void makeImageViews(size_t gpuIdx, vk::ImageView* (&out)[2]) {
      if constexpr(SLOTS[0].type == GpuResourceType::eImage && (SLOTS[0].getUsage() & usage))
	for(size_t i = 0;i < 2;i++)
	  (*out[i]++) = data.swapper.all()[i].getVkImageView(gpuIdx);
      if constexpr(L > 1)
	rest.template makeImageViews<usage>(gpuIdx, out);
    };

    template<usage_t usage, GpuResourceType type> inline auto& first() {
      if constexpr(SLOTS[0].type == type && (SLOTS[0].getUsage() & usage))
	return data;
      else
	return rest.template first<usage, type>();
    };

  };

  template<Collections::LiteralList<GpuResourceSlotInfo> SLOTS> struct BackingTuple<SLOTS, 0> {};//empty

}
