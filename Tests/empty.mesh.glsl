#version 450
#extension GL_EXT_mesh_shader : enable

layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;
layout(triangles, max_vertices=256, max_primitives=256) out;

layout (std140, set = 0, binding = 0) uniform rendbuf {
  vec4 locr;
} source;

layout(std140, set = 1, binding = 0) uniform target_t {
  vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
} target;

layout(std140, set = 1, binding = 1) uniform tt_t {
  mat4 transform;
} targetTrans;

void main() {
  SetMeshOutputsEXT(0, 0);
}

