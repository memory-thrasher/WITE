#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (std140, set = 1, binding = 0) uniform cambuf {
  dmat4 trans;
} target;
layout (std140, set = 0, binding = 0) uniform rendbuf {
  dmat4 trans;
} source;
layout (location = 0) in vec3 pos;
layout (location = 0) out vec3 norm;
layout (location = 1) out vec3 worldPos;
void main() {
  worldPos = vec3((source.trans * vec4(pos, 1)).xyz);
  gl_Position = vec4(target.trans * vec4(worldPos, 1));
  norm = vec3(normalize(worldPos.xyz - source.trans[3].xyz));
}


