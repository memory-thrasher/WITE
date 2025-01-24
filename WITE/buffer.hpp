/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include "wite_vulkan.hpp"
#include "gpu.hpp"
#include "onionTemplateStructs.hpp"
#include "udm.hpp"
#include "stdExtensions.hpp"
#include "thread.hpp"
#include "onionUtils.hpp"

namespace WITE {

  template<bufferRequirements R> struct buffer;

  struct perThreadStagingBuffer {
    static constexpr uint32_t stagingBufferSize = 4096;
    template<uint64_t gpuId> struct requirementsFor {
      static constexpr bufferRequirements value {
	.deviceId = gpuId,
	.usage = vk::BufferUsageFlagBits::eTransferSrc,
	.size = stagingBufferSize,
	.frameswapCount = 1,
	.hostVisible = true,
      };
    };
    static threadResource<perThreadStagingBuffer> all;
    std::map<uint64_t, void*> buffersByGpu;
    template<uint64_t gpu> static buffer<requirementsFor<gpu>::value>& get() {
      auto*& ret = *reinterpret_cast<buffer<requirementsFor<gpu>::value>**>(&all.get()->buffersByGpu[gpu]);
      if(!ret)
	ret = new buffer<requirementsFor<gpu>::value>();
      return *ret;
    };
  };

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
	VK_ASSERT(dev.getVkDevice().bindBufferMemory(vkBuffer[i], rams[i].handle, 0), "failed to bind buffer memory");
	// WARN("bound ram ", rams[i].handle, " to buffer ", vkBuffer[i]);
      }
    };

    ~buffer() {
      gpu& dev = gpu::get(R.deviceId);
      for(size_t i = 0;i < R.frameswapCount;i++) {
	dev.getVkDevice().destroyBuffer(vkBuffer[i], ALLOCCB);
	rams[i].free();
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
	//mutex-free bc each thread gets its own staging area
	auto& staging = perThreadStagingBuffer::get<R.deviceId>();
	vk::BufferCopy copy;
	copy.srcOffset = 0;
	vk::Buffer vkStaging = staging.frameBuffer(0);
	auto dev = gpu::get(R.deviceId).getVkDevice();
	void* data;
	for(uint32_t offset = 0;offset < sizeof(T);offset += perThreadStagingBuffer::stagingBufferSize) {
	  uint32_t batchSize = min(sizeof(T) - offset, perThreadStagingBuffer::stagingBufferSize);
	  VK_ASSERT(dev.mapMemory(staging.rams[0].handle, 0, batchSize, {}, &data), "Failed to map memory.");
	  std::memcpy(data, (void*)((uint8_t*)&t + offset), batchSize);
	  dev.unmapMemory(staging.rams[0].handle);
	  copy.size = batchSize;
	  copy.dstOffset = offset;
	  auto cmd = gpu::get(R.deviceId).getTempCmd();
	  for(uint8_t i = 0;i < R.frameswapCount;i++)
	    if((1 << i) & frameMask)
	      cmd->copyBuffer(vkStaging, frameBuffer(i), 1, &copy);
	  cmd.submit();
	  cmd.waitFor();
	}
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
