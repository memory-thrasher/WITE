#version 450
#extension GL_EXT_mesh_shader : enable

//x = node position along leaf axis, y = top vs bottom, z = face id within that node
layout(local_size_x = 5, local_size_y = 2, local_size_z = 25) in;
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

vec4 project(vec4 pos) {
  return project(pos.xyz);
}

const float PI = 3.1415926535897932384626f;
const float segmentTiltAngle = 15*PI/180;

void main() {
  SetMeshOutputsEXT(250, 250);
  const uint faceId = gl_LocalInvocationID.z;
  const uint baseFaceIdx = (gl_LocalInvocationID.x * 2 + gl_LocalInvocationID.y) * 25;//TODO tune
  const uint baseVertIdx = (gl_LocalInvocationID.x * 2 + gl_LocalInvocationID.y) * 25;//TODO tune
  const float sideSign = sign(gl_LocalInvocationID.y-0.5f);
  const float scale = 25.0f/(gl_LocalInvocationID.x * gl_LocalInvocationID.x + 25);
  const mat4 segmentMat = mat4(cos(segmentTiltAngle)*sqrt(2)/2*scale, sin(segmentTiltAngle), sideSign*sqrt(2)/2*scale, 0,
			       -sin(segmentTiltAngle)*scale, cos(segmentTiltAngle), 0, 0,
			       -sqrt(2)/2*scale, 0, sideSign*sqrt(2)/2*scale, 0,
			       2.5f*gl_LocalInvocationID.x, 0, scale*(int(gl_LocalInvocationID.y*2)-1), 1);
  if(faceId < 16) {
    gl_MeshVerticesEXT[baseVertIdx].gl_Position = project(segmentMat * vec4(0, -0.25f, 0, 1));
    const uint xd = faceId <= 8 ? faceId : 16 - faceId, yd = xd & 7;
    gl_MeshVerticesEXT[baseVertIdx + faceId + 1].gl_Position = project(segmentMat * vec4(xd * (3 / 8.0f) - 1, 0.1f * (faceId&1), (sin(yd*(3*PI/16.0f))*0.75f + yd/6.0f) * sign(8-int(faceId)), 1));
    gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId] = uvec3(0, faceId == 15 ? 1 : faceId+2, faceId+1) +
      uvec3(baseVertIdx, baseVertIdx, baseVertIdx);
  } else {
    //
  }
}

