#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (std140, binding = 0) uniform cambuf {
  mat4 trans;
} camera;
// layout (std140, binding = 1) uniform buf {
// } shaderInstance;
layout (std140, binding = 2) uniform rendbuf {
  mat4 trans;
} renderable;
layout (location = 0) in vec4 pos;
layout (location = 0) out vec3 norm;
void main() {
  gl_Position = camera.trans * renderable.trans * pos;
  norm = normalize(pos.xyz);
}


