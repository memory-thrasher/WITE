#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_shader_subgroup_shuffle_relative : enable

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D inImg;

layout (constant_id = 0) const int vertical = 0;//0 or 1

void main() {
  const vec2 size = textureSize(inImg, 0);
  const uint maxFlare = 4;
  // const uvec2 target = uvec2(inUV * target.size.xy + (0.5f).xx);
  const vec2 pxlStride = vec2((vertical^1)/size.x, vertical/size.y);
  vec3 pxl = texture(inImg, inUV).xyz;
  for(uint i = 1;i <= maxFlare;i++) {
    const float den = 1.0f/(i*i + 1);
    pxl += clamp(texture(inImg, inUV + i*pxlStride).xyz - (1.0f).xxx, (0.0f).xxx, (1.0f).xxx) * den;
    pxl += clamp(texture(inImg, inUV - i*pxlStride).xyz - (1.0f).xxx, (0.0f).xxx, (1.0f).xxx) * den;
  }
  // float mx = max(max(pxl.x, pxl.y), pxl.z);
  // if(mx > 1) pxl /= mx;
  outColor = vec4(pxl, 1);
}
