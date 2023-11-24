#pragma once

#include "templateStructs.hpp"
#include "onionUtils.hpp"
#include "buffer.hpp"
#include "image.hpp"

namespace WITE {

  template<literalList<shader> S, shaderTargetLayout TL, uint64_t ONION_ID, uint64_t GPUID = 0> struct onion {

    template<uint64_t id> inline static constexpr auto imageRequirements_v = getImageRequirements<id, ONION_ID, S, GPUID>();

    template<uint64_t id> inline static constexpr auto bufferRequirements_v = getBufferRequirements<id, TL, S, GPUID>();

    template<shaderTargetInstanceLayout IL> class target_t {
    private:
      bufferBase* vertexBuffers[TL.vertexBuffers.len];
      bufferBase* instanceBuffers[TL.instanceBuffers.len];
      bufferBase* uniformBuffers[TL.uniformBuffers.len];
      imageBase* sampledImages[TL.sampled.len];
      imageBase* attachments[TL.attachments.len];

      target_t(const target_t&) = delete;
      target_t() = default;

      friend onion;

    public:

      ~target_t() = default;

      template<uint64_t ID, bufferRequirements R> void setVertexBuffer(buffer<R>* b) {
	constexpr static uint64_t idx = findId(TL.vertexBuffers, ID);
	static_assert_show(bufferRequirements_v<ID> <= R, bufferRequirements_v<ID>);
	vertexBuffers[idx] = b;
      };

      template<uint64_t ID, bufferRequirements R> void setInstanceBuffer(buffer<R>* b) {
	constexpr static uint64_t idx = findId(TL.instanceBuffers, ID);
	static_assert_show(bufferRequirements_v<ID> <= R, bufferRequirements_v<ID>);
	instanceBuffers[idx] = b;
      };

      template<uint64_t ID, bufferRequirements R> void setUniformBuffer(buffer<R>* b) {
	constexpr static uint64_t idx = findId(TL.uniformBuffers, ID);
	static_assert_show(bufferRequirements_v<ID> <= R, bufferRequirements_v<ID>);
	uniformBuffers[idx] = b;
      };

      template<uint64_t ID, imageRequirements R> void setSampledImage(image<R>* b) {
	constexpr static uint64_t idx = findId(TL.sampled, ID);
	static_assert_show(imageRequirements_v<ID> <= R, imageRequirements_v<ID>);
	sampledImages[idx] = b;
      };

      template<uint64_t ID, imageRequirements R> void setAttachment(image<R>* b) {
	constexpr static uint64_t idx = findId(TL.attachments, ID);
	static_assert_show(imageRequirements_v<ID> <= R, imageRequirements_v<ID>);
	attachments[idx] = b;
      };

    };

    template<shaderTargetInstanceLayout IL> target_t<IL> createTarget() {
      return {};
    };

    template<uint64_t SHADER_ID> class source_t {
    public:
      static constexpr size_t SHADER_IDX = findId(S, SHADER_ID);
      static constexpr shader SHADER_PARAMS = S[SHADER_IDX];

    private:
      bufferBase* vertexBuffer;
      bufferBase* instanceBuffer;
      bufferBase* uniformBuffers[SHADER_PARAMS.uniformBuffers.len];
      imageBase* sampledImages[SHADER_PARAMS.sampled.len];

      friend onion;

    public:

      template<bufferRequirements R> void setVertexBuffer(buffer<R>* b) {
	static_assert_show(SHADER_PARAMS.vertexBuffer && bufferRequirements_v<SHADER_PARAMS.vertexBuffer->id> <= R,
			   SHADER_PARAMS.vertexBuffer);
	vertexBuffer = b;
      };

      template<bufferRequirements R> void setInstanceBuffer(buffer<R>* b) {
	static_assert_show(SHADER_PARAMS.instanceBuffer && bufferRequirements_v<SHADER_PARAMS.instanceBuffer->id> <= R,
			   SHADER_PARAMS.instanceBuffer);
	instanceBuffer = b;
      };

      template<uint64_t ID, bufferRequirements R> void setUniformBuffer(buffer<R>* b) {
	constexpr static uint64_t idx = findId(SHADER_PARAMS.uniformBuffers, ID);
	static_assert_show(bufferRequirements_v<ID> <= R, bufferRequirements_v<ID>);
	uniformBuffers[idx] = b;
      };

      template<uint64_t ID, imageRequirements R> void setSampledImage(image<R>* b) {
	constexpr static uint64_t idx = findId(SHADER_PARAMS.sampled, ID);
	static_assert_show(imageRequirements_v<ID> <= R, imageRequirements_v<ID>);
	sampledImages[idx] = b;
      };

    };

    template<uint64_t shaderId> source_t<shaderId> createSource() {
      return {};
    };

  };

};
