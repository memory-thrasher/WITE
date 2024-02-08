#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (std140, set = 1, binding = 0) uniform cambuf {
  vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
  mat4 transform;
} target;
layout (std140, set = 0, binding = 0) uniform rendbuf {
  dmat4 trans;
} source;
layout (location = 0) in vec3 pos;
layout (location = 0) out vec3 norm;
layout (location = 1) out vec3 worldPos;
void main() {
  worldPos = vec3((source.trans * vec4(pos, 1)).xyz);
  vec3 camPos = (target.transform * vec4(worldPos, 1)).xyz;
  gl_Position = vec4(camPos.xy * target.clip.wz * vec2(-1, 1) / camPos.z,
		     (-camPos.z - target.clip.x) / (target.clip.y - target.clip.x), 1);
  norm = vec3(normalize(worldPos.xyz - source.trans[3].xyz));
}


