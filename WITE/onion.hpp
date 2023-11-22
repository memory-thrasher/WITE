#pragma once

#include "templateStructs.hpp"
#include "onionUtils.hpp"
#include "buffer.hpp"
#include "image.hpp"

namespace WITE {

  template<literalList<shader> S, shaderTargetLayout TL, uint64_t ONION_ID, uint64_t GPUID = 0> struct onion {

    template<uint64_t id> inline static constexpr auto imageRequirements_v = getImageRequirements<id, ONION_ID, S, GPUID>();

    template<uint64_t id> inline static constexpr auto bufferRequirements_v = getBufferRequirements<id, TL, S, GPUID>();

    template<shaderTargetInstanceLayout IL> class camera_t {
    private:
      bufferBase* vertexBuffers[TL.vertexBuffers.len];
      bufferBase* instanceBuffers[TL.instanceBuffers.len];
      bufferBase* uniformBuffers[TL.uniformBuffers.len];
      imageBase* sampledImages[TL.sampled.len];
      imageBase* attachments[TL.attachments.len];
      glm::uvec4 size;//which components are used depends on IL. cube = x, 2d = xy, 3d = xyz, 4d = xyzw (if ever implemented)

      camera_t() = delete;
      camera_t(const camera_t&) = delete;

    public:

      camera_t(glm::uvec4 size) : size(size) {};

      template<uint64_t ID, bufferRequirements R> void setVertexBuffer(buffer<R>* b) {
	constexpr static uint64_t idx = findId(TL.vertexBuffers, ID);
	static_assert(satisfies<R>(TL.vertexBuffers[idx]));
	vertexBuffers[idx] = b;
      };

      template<uint64_t ID, bufferRequirements R> void setInstanceBuffer(buffer<R>* b) {
	constexpr static uint64_t idx = findId(TL.instanceBuffers, ID);
	static_assert(satisfies<R>(TL.instanceBuffers[idx]));
	instanceBuffers[idx] = b;
      };

      template<uint64_t ID, bufferRequirements R> void setUniformBuffer(buffer<R>* b) {
	constexpr static uint64_t idx = findId(TL.uniformBuffers, ID);
	static_assert(satisfies<R>(TL.uniformBuffers[idx]));
	uniformBuffers[idx] = b;
      };

      template<uint64_t ID, imageRequirements R> void setAttachment(image<R>* b) {
	constexpr static uint64_t idx = findId(TL.attachments, ID);
	static_assert(satisfies<R>(TL.attachments[idx]));
	attachments[idx] = b;
      };

      template<uint64_t ID, imageRequirements R> void setSampledImage(image<R>* b) {
	constexpr static uint64_t idx = findId(TL.sampled, ID);
	static_assert(satisfies<R>(TL.sampled[idx]));
	sampledImages[idx] = b;
      };

    };

    template<shaderTargetInstanceLayout IL> camera_t<IL> createTarget(unsigned int x) {
      static_assert(IL.targetType == targetType_t::eCube);
      return {{ x, 0, 0, 0 }};
    };

    template<shaderTargetInstanceLayout IL> camera_t<IL> createTarget(glm::uvec2 v) {
      static_assert(IL.targetType == targetType_t::e2D);
      return {{ v.x, v.y, 0, 0 }};
    };

    template<shaderTargetInstanceLayout IL> camera_t<IL> createTarget(glm::uvec3 v) {
      static_assert(IL.targetType == targetType_t::e3D);
      return {{ v.x, v.y, v.z, 0 }};
    };

  };

};
