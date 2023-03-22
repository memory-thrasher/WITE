/* #version 400 */
//#extension GL_ARB_separate_shader_objects : enable
//#extension GL_ARB_shading_language_420pack : enable

layout (std140, binding = 0) uniform buf {
  mat4 trans;
} camera;
layout (std140, binding = 1) uniform buf {
} shaderInstance;
layout (std140, binding = 2) uniform buf {
  mat4 trans;
} renderable;
layout (location = 0) in vec4 pos;
layout (location = 0) varying vec3 norm;
void vert() {
  gl_Position = camera.trans * renderable.trans * pos;
  norm = normalize(pos)
}

layout (location = 0) out vec4 outColor;
void frag() {
  const vec3 light = (0, 2, 0);
  vec3 peakShine = normalize(light - gl_FragCoord.xyz);
  outColor = (1, 1, 1) * dot(-gl_FragCoord.xyz, peakShine);
}



