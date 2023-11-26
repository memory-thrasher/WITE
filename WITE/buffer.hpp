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
    std::atomic_uint64_t lastUpdated = 0;

    buffer() {
      gpu& dev = gpu::get(R.deviceId);
      for(size_t i = 0;i < R.frameswapCount;i++) {
	VK_ASSERT(dev.getVkDevice().createBuffer(&ci, ALLOCCB, &vkBuffer[i]), "failed to create buffer");
	dev.getVkDevice().getBufferMemoryRequirements(vkBuffer[i], &mrs[i]);
	vk::MemoryPropertyFlags flags;
	if constexpr(R.hostVisible) {
	  flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
	} else {
	  flags = vk::MemoryPropertyFlagBits::eHostCoherent;
	}
	dev.allocate(mrs[i], flags, &rams[i]);
      }
    };

    size_t frameBufferIdx(int64_t frame) const {
      ASSERT_TRAP(frame >= -R.frameswapCount, "requested frame too far before frame 0");
      if(frame < 0) [[unlikely]] frame += R.frameswapCount;
      return frame % R.frameswapCount;
    };

    virtual vk::Buffer frameBuffer(int64_t frame) const override {
      return vkBuffer[frameBufferIdx(frame)];
    };

    //frame is important to decide which buffer is to be used. Frame may be a lie, especially a negative number for setting up initial values.
    template<class T> void setOnFrame(int64_t frame, const T& t) {
      static_assert(R.hostVisible);
      constexpr struct { size_t t = sizeof(T), r = R.size; } debugdata;
      static_assert_show(sizeof(T) == R.size, debugdata);
      size_t idx = frameBuffer(frame);
      auto dev = gpu::get(R.deviceId).getVkDevice();
      void* data;
      VK_ASSERT(dev.mapMemory(rams[idx], 0, R.size, {}, &data), "Failed to mep memory.");
      memcpy(data, t);
      dev.unmapMemory(rams[idx]);
      lastUpdated = frame;
    };

    template<udm U = R.format, class T = copyableArray<udmObject<U>, R.size / sizeofUdm<U>()>> void set(int64_t frame, const T& src) {
      static_assert((R.size % sizeofUdm<U>()) == 0);
      set<T>(frame, src);
    };

    // //waits for frame before pulling the data.
    // template<class T> T getOnFrame(int64_t frame) {
    //   static_assert(R.hostVisible);
    //   int64_t currentFrame = 0;
    //   ASSERT_TRAP(frame <= currentFrame+1 && frame > currentFrame - R.frameswapCount, "Invalid frame ", frame);
    //   static_assert(false);//TODO
    // };

  };

}
