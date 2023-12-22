#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (std140, set = 1, binding = 0) uniform cambuf {
  mat4 trans;
} target;
layout (std140, set = 0, binding = 0) uniform rendbuf {
  mat4 trans;
} source;
layout (location = 0) in vec3 pos;
layout (location = 0) out vec3 norm;
void main() {
  // gl_Position = target.trans * (source.trans * vec4(pos, 1));
  gl_Position = vec4(pos, 1);
  norm = normalize(pos.xyz);
}


