#version 450

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNorm;
layout(location = 2) out vec3 outWorldPos;

layout(std140, set = 1, binding = 0) uniform target_t {
  vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
} target;

layout(std140, set = 1, binding = 1) uniform tt_t {
  mat4 transform;
} targetTrans;

layout(std140, set = 0, binding = 0) uniform sourcebuf {
  mat4 trans;
} source;

//always draw with 36 verts. clockwise wound.
void main() {
  const int faceId = gl_VertexIndex / 6;
  const int vertId = gl_VertexIndex % 6;
  const int axisId = faceId >> 1;//0=x 1=y 2=z
  const int reversed = faceId & 1;
  const int forward = reversed ^ 1;
  const ivec3 localZ = ivec3(axisId == 0, axisId == 1, axisId == 2);
  const ivec2 faceLocalPos = ivec2(vertId&1, vertId < 5 && vertId > 1);//each component is 0 or 1
  const vec3 localPos = vec3(ivec3(axisId != 0, 0, axisId == 0) * (faceLocalPos.x ^ reversed ^ localZ.z) +
			     ivec3(0, axisId != 1, axisId == 1) * faceLocalPos.y +
			     localZ * forward) * 2 - vec3(1, 1, 1);
  const vec3 localNorm = localZ * (forward * 2 - 1);
  const vec4 worldPos = source.trans * vec4(localPos, 1);
  outWorldPos = worldPos.xyz;
  const vec3 camPos = (targetTrans.transform * worldPos).xyz;
  //abs prevents one coordinate from ending up on the other side of the screen when only part of the triangle is behind the camera
  gl_Position = vec4(camPos.xy * target.clip.wz * vec2(-1, 1) / -abs(camPos.z),
		     (-camPos.z - target.clip.x) / (target.clip.y - target.clip.x), 1);
  outNorm = (source.trans * vec4(localNorm, 0)).xyz;
  outUV = vec2(faceLocalPos);
}

/*
0 1
2

  5
4 3

x is 1 for: 1, 3, 5 = f&1
y is 1 for: 2, 3, 4 = f>1 && f<5

face order: x+ x- y+ y- z+ z-
object-local axis that are mapped to face-local axis by face id in object-local space, followed by the face-local oriented by object-local origin relative z coord:
x+:  z  y  1
x-: -z  y  0
y+:  x  z  1
y-: -x  z  0
z+:  x  y  1
z-: -x  y  0

*/

