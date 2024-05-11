#version 450
#extension GL_EXT_mesh_shader : enable

layout(local_size_x = 16, local_size_y = 1, local_size_z = 1) in;
layout(triangles, max_vertices=256, max_primitives=256) out;

layout (std140, set = 0, binding = 0) uniform sharedData {
  mat4 trans;
} source;

layout(std140, set = 1, binding = 0) uniform target_t {
  vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
} target;

layout(std140, set = 1, binding = 1) uniform tt_t {
  mat4 transform;
} targetTrans;

vec4 project(vec3 pos) {
  const vec4 worldPos = source.trans * vec4(pos, 1);
  const vec3 camPos = (targetTrans.transform * worldPos).xyz;
  return vec4(camPos.xy * target.clip.wz * vec2(-1, 1) / camPos.z, (-camPos.z - target.clip.x) / (target.clip.y - target.clip.x), 1);
}

#define PI 3.1415926535897932384626f

void main() {
  SetMeshOutputsEXT(17, 16);
  const uint faceId = gl_LocalInvocationID.x;
  gl_MeshVerticesEXT[0].gl_Position = project(vec3(0, -0.25f, 0));
  const uint xd = faceId <= 8 ? faceId : 16 - faceId, yd = xd & 7;
  gl_MeshVerticesEXT[faceId+1].gl_Position = project(vec3(xd * (3 / 8.0f) - 1,
							  0.1f * (faceId&1),
							  (sin(yd*(3*PI/16.0f))*0.75f + yd/6.0f) * sign(8-int(faceId))));
  gl_PrimitiveTriangleIndicesEXT[faceId] = uvec3(0, faceId == 15 ? 1 : faceId+2, faceId+1);
}

