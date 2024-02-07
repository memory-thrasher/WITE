#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 norm;
layout (location = 1) in vec3 worldPos;
// layout (location = 2) in vec4 realOut;
layout (location = 0) out vec4 outColor;
void main() {
  const vec3 light = { 0, 2, 1 };
  vec3 peakShine = normalize(light - worldPos);
  // outColor = vec4(gl_FragCoord.z, 0, realOut.z-12, 1);
  outColor = (dot(normalize(norm), peakShine)/2 + 0.5) * vec4(1, 0.5, 0, 1);
}
