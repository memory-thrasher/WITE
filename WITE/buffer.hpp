#pragma once

#include "wite_vulkan.hpp"
#include "gpu.hpp"
#include "templateStructs.hpp"
#include "udm.hpp"
#include "stdExtensions.hpp"

namespace WITE {

  template<bufferRequirements R> struct buffer {
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
	  flags = vk::MemoryPropertyFlagBits::eHostVisible;
	} else {
	  flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
	}
	dev.allocate(mrs[i], flags, &rams[i]);
	dev.getVkDevice().bindBufferMemory(vkBuffer[i], rams[i].handle, 0);
	// WARN("bound ram ", rams[i].handle, " to buffer ", vkBuffer[i]);
      }
    };

    size_t frameBufferIdx(int64_t frame) const {
      ASSERT_TRAP(frame >= -R.frameswapCount, "requested frame too far before frame 0");
      if(frame < 0) [[unlikely]] frame += R.frameswapCount;
      return frame % R.frameswapCount;
    };

    inline uint64_t frameUpdated(uint64_t currentFrame) {
      return 0;//NYI for buffers. Buffers not resizable (and why would you?)
    };

    // template<resizeBehavior RB, resizeReference RR> inline void applyPendingResize(uint64_t frame, vk::CommandBuffer cmd, garbageCollector& gc) {
    //   //NYI for buffers
    // };

    vk::Buffer frameBuffer(int64_t frame) const {
      return vkBuffer[frameBufferIdx(frame)];
    };

    //frame is important to decide which buffer is to be used. Frame may be a lie, especially a negative number for setup
    template<class T> void set(int64_t frame, const T& t) {
      static_assert(R.hostVisible);
      constexpr struct { size_t t = sizeof(T), r = R.size; } debugdata;
      static_assert_show(sizeof(T) == R.size, debugdata);
      size_t idx = frameBufferIdx(frame);
      auto dev = gpu::get(R.deviceId).getVkDevice();
      void* data;
      VK_ASSERT(dev.mapMemory(rams[idx].handle, 0, R.size, {}, &data), "Failed to map memory.");
      std::memcpy(data, (void*)&t, R.size);
      dev.unmapMemory(rams[idx].handle);
      lastUpdated = frame;
      // WARN("wrote ", sizeof(T), " bytes to vram ", rams[idx].handle);
    };

    template<class T> void slowOutOfBandSet(const T& t, uint64_t frameMask = 0xFFFFFFFFFFFFFFFF) {
      constexpr struct { size_t t = sizeof(T), r = R.size; } debugdata;
      static_assert_show(sizeof(T) == R.size, debugdata);
      auto dev = gpu::get(R.deviceId).getVkDevice();
      void* data;
      //for static buffers. Do not use while a render is in progress unless you know that the buffers in use are not in the mask
      //even then, recreating a buffer regularly is just bad
      if constexpr(R.hostVisible) {//TODO || runtime memory type happens to be host visible anyway
	for(uint8_t i = 0;i < R.frameswapCount;i++) {
	  if((1 << i) & frameMask) {
	    VK_ASSERT(dev.mapMemory(rams[i].handle, 0, R.size, {}, &data), "Failed to map memory.");
	    memcpy(data, t);
	    dev.unmapMemory(rams[i].handle);
	  }
	}
      } else {
	static constexpr bufferRequirements tempBufferRequirements = stagingRequirementsFor(R);
	static_assert(tempBufferRequirements.size == R.size);
	static constexpr vk::BufferCopy copy(0, 0, R.size);
	buffer<tempBufferRequirements> temp;
	vk::Buffer src = temp.frameBuffer(0);
	temp.set(0, t);
	auto cmd = gpu::get(R.deviceId).getTempCmd();
	for(uint8_t i = 0;i < R.frameswapCount;i++)
	  if((1 << i) & frameMask)
	    cmd->copyBuffer(src, frameBuffer(i), 1, &copy);
	cmd.submit();
	cmd.waitFor();
      }
    };

    // template<class T> void setAll(const T& t) {
    //   static_assert(R.hostVisible);
    //   constexpr struct { size_t t = sizeof(T), r = R.size; } debugdata;
    //   static_assert_show(sizeof(T) == R.size, debugdata);
    //   auto dev = gpu::get(R.deviceId).getVkDevice();
    //   void* data;
    //   for(size_t idx = 0;idx < R.frameswapCount;idx++) {
    // 	VK_ASSERT(dev.mapMemory(rams[idx], 0, R.size, {}, &data), "Failed to mep memory.");
    // 	memcpy(data, t);
    // 	dev.unmapMemory(rams[idx]);
    //   }
    // };

    // //waits for frame before pulling the data.
    // template<class T> T getOnFrame(int64_t frame) {
    //   static_assert(R.hostVisible);
    //   int64_t currentFrame = 0;
    //   ASSERT_TRAP(frame <= currentFrame+1 && frame > currentFrame - R.frameswapCount, "Invalid frame ", frame);
    //   static_assert(false);//TODO
    // };

  };

}
