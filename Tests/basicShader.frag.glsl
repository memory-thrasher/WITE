#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 norm;
layout (location = 0) out vec4 outColor;
void main() {
  // const vec3 light = { 0, 2, 0 };
  // vec3 peakShine = normalize(light - gl_FragCoord.xyz);
  // outColor = vec4(1, 1, 1, 1) * dot(norm, peakShine);
  outColor = vec4(1, 1, 1, 1);
}
