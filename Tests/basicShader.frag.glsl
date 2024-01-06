#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 norm;
layout (location = 1) in vec3 worldPos;
layout (location = 0) out vec4 outColor;
void main() {
  const vec3 light = { 0, 2, 1 };
  vec3 peakShine = normalize(light - worldPos);
  outColor = vec4(1, 1, 1, 1) * (dot(normalize(norm), peakShine)/2 + 0.5);
}
