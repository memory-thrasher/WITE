#pragma once

#include "wite_vulkan.hpp"
#include "gpu.hpp"
#include "templateStructs.hpp"
#include "udm.hpp"
#include "stdExtensions.hpp"

namespace WITE {

  struct bufferBase {
    virtual ~bufferBase() = default;
    virtual vk::Buffer frameBuffer(int64_t frame) const = 0;
  };

  template<bufferRequirements R> struct buffer : public bufferBase {
    static_assert_show(isValid(R), R);
    static constexpr vk::BufferCreateInfo ci = getDefaultCI(R);

    vk::Buffer vkBuffer[R.frameswapCount];
    vk::MemoryRequirements mrs[R.frameswapCount];
    gpu::vram rams[R.frameswapCount];
    uint8_t data[R.size];
    std::atomic_bool dirty;

    buffer() {
      gpu& dev = gpu::get(R.deviceId);
      for(size_t i = 0;i < R.frameswapCount;i++) {
	VK_ASSERT(dev.getVkDevice().createBuffer(&ci, ALLOCCB, &vkBuffer[i]), "failed to create buffer");
	dev.getVkDevice().getBufferMemoryRequirements(vkBuffer[i], &mrs[i]);
	dev.allocate(mrs[i], R.memoryFlags, &rams[i]);
      }
    };

    virtual vk::Buffer frameBuffer(int64_t frame) const override {
      ASSERT_TRAP(frame >= -R.frameswapCount, "requested frame too far before frame 0");
      if(frame < 0) [[unlikely]] frame += R.frameswapCount;
      return vkBuffer[frame % R.frameswapCount];
    };

    template<class T> void set(const T& t) {
      constexpr struct { size_t t = sizeof(T), r = R.size; } debugdata;
      static_assert_show(sizeof(T) == R.size, debugdata);
      memcpy(data, t);
      dirty = true;
    };

    template<udm U, class T = copyableArray<udmObject<U>, R.size / sizeofUdm<U>()>> void set(const T& src) {
      static_assert((R.size % sizeofUdm<U>()) == 0);
      set<T>(src);
    };

  };

}
