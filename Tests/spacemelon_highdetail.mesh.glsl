#version 450
#extension GL_EXT_mesh_shader : enable

//x = node position along leaf axis, y = face id within that node
layout(local_size_x = 5, local_size_y = 25, local_size_z = 1) in;
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

const float PI = 3.1415926535897932384626f;
const float segmentTiltAngle = 15*PI/180;
const uint vertsPerNode = 45, facesPerNode = 50, finalSegmentVerts = 44, finalSegmentFaces = 45;

vec4 project(vec3 pos) {
  const vec4 worldPos = source.trans * vec4(pos, 1);
  const vec3 camPos = (targetTrans.transform * worldPos).xyz;
  return vec4(camPos.xy * target.clip.wz * vec2(-1, 1) / -abs(camPos.z), (-camPos.z - target.clip.x) / (target.clip.y - target.clip.x), 1);
}

vec4 project(vec4 pos) {
  return project(pos.xyz);
}

float iterOffset(uint iter) {
  return 2.5f*iter;
}

vec4 transformFor(vec3 p, uint iter) {
  const float scale = 25.0f/(iter * iter + 25);
  return mat4(cos(segmentTiltAngle)*sqrt(2)/2*scale, sin(segmentTiltAngle), sqrt(2)/2*scale, 0,
	      -sin(segmentTiltAngle)*scale, cos(segmentTiltAngle), 0, 0,
	      -sqrt(2)/2*scale, 0, sqrt(2)/2*scale, 0,
	      iterOffset(iter), 0, scale*2, 1) * vec4(p, 1);
}

vec4 pForFaceId(uint pointId, uint iter) {
  vec3 p;
  if(pointId == 0)
    p = vec3(0, -0.25f, 0);
  else {
    const uint faceIdForP = pointId - 1;
    const uint xd = faceIdForP <= 8 ? faceIdForP : 16 - faceIdForP, yd = xd & 7;
    p = vec3(xd * (3 / 8.0f) - 1, 0.1f * (faceIdForP&1), (sin(yd*(3*PI/16.0f))*0.75f + yd/6.0f) * sign(8-int(faceIdForP)));
  }
  return transformFor(p, iter);
}

void main() {
  SetMeshOutputsEXT(vertsPerNode*4 + finalSegmentVerts, facesPerNode*4 + finalSegmentFaces);
  const uint faceId = gl_LocalInvocationID.y;
  const uint baseFaceIdx = gl_LocalInvocationID.x * facesPerNode;
  const uint baseVertIdx = gl_LocalInvocationID.x * vertsPerNode;
  const vec4 p = pForFaceId(faceId < 16 ? faceId+1 : gl_LocalInvocationID.x == 4 ? 16 : 15, gl_LocalInvocationID.x);
  const vec4 q = pForFaceId(faceId < 16 ? 0 : 1, gl_LocalInvocationID.x + (faceId < 16 ? 0 : 1));
  if(faceId < 16) {//consumes 16 of every 25 invocations, 34 of every 50 vertices, and 32 of every 50 triangles
    //each invocation draws two triangles, mirrored across the XZ plane
    if(faceId == 0) {
      gl_MeshVerticesEXT[baseVertIdx].gl_Position = project(q);
      gl_MeshVerticesEXT[baseVertIdx+17].gl_Position = project(q * vec4(1, 1, -1, 1));
    }
    gl_MeshVerticesEXT[baseVertIdx + faceId + 1].gl_Position = project(p);
    gl_MeshVerticesEXT[baseVertIdx + faceId + 18].gl_Position = project(p * vec4(1, 1, -1, 1));
    gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId] = uvec3(0, faceId == 15 ? 1 : faceId+2, faceId+1) +
      uvec3(1, 1, 1) * baseVertIdx;
    gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId + 16] = uvec3(0, faceId == 15 ? 1 : faceId+2, faceId+1) +
      uvec3(1, 1, 1) * (baseVertIdx + 17);
  } else if(gl_LocalInvocationID.x == 4) {//endcap
    if(faceId < 22) {
      if(faceId == 16)
	gl_MeshVerticesEXT[baseVertIdx + 34].gl_Position = project(vec3(iterOffset(gl_LocalInvocationID.x) - 1, -sin(segmentTiltAngle), 0));
      if(faceId > 16) {
	const float y = (21 - faceId) / 5.0f;
	const vec3 interp = p.xyz * vec3(1, 1, y) + vec3(((y*y)-1)*0.5f, (~faceId&1) * 0.05f, 0);
	gl_MeshVerticesEXT[baseVertIdx + faceId + 18].gl_Position = project(interp);
	if(faceId < 24)
	  gl_MeshVerticesEXT[baseVertIdx + faceId + 23].gl_Position = project(interp * vec3(1, 1, -1));
      }
      gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId + 16] =
	uvec3(baseVertIdx + 34,
	      baseVertIdx + (faceId < 18 ? 16 : faceId + 17),
	      baseVertIdx + (faceId == 16 ? 1 : faceId + 18));
      gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId + 23] =
	uvec3(baseVertIdx + 34,
	      baseVertIdx + (faceId < 18 ? 33 : faceId + 22),
	      baseVertIdx + (faceId == 16 ? 18 : faceId < 24 ? faceId + 23 : 39));
    }
  } else {
    if(faceId == 16)
      gl_MeshVerticesEXT[baseVertIdx + 34].gl_Position = project(vec3(iterOffset(gl_LocalInvocationID.x) - 1, -sin(segmentTiltAngle), 0));
    if(faceId > 16 && faceId < 22) {
      const float nextScale = 25.0f/(pow(gl_LocalInvocationID.x + 1, 2) + 25),
	a = (faceId - 16.0f) / 6;
      const vec3 nodeConDelta = q.xyz - p.xyz;
      const vec3 interp = mix(p.xyz, q.xyz, a) +
	cross(nodeConDelta, vec3(0, -1, 0)) * (0.25f-pow(a-0.5f, 2)) +
	cross(nodeConDelta, vec3(0, 0, 1)) * ((faceId&1) * 0.05f);
      gl_MeshVerticesEXT[baseVertIdx + faceId + 18].gl_Position = project(interp);
      gl_MeshVerticesEXT[baseVertIdx + faceId + 23].gl_Position = project(interp * vec3(1, 1, -1));
    }
    gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId + 16] =
      uvec3(baseVertIdx + (faceId < 22 ? 34 : faceId + 15),
	    baseVertIdx + (faceId < 18 ? 16 : faceId < 21 ? faceId + 17 : 34 + vertsPerNode),
	    baseVertIdx + (faceId == 16 ? 1 : faceId < 19 ? 15 : faceId < 24 ? faceId + 16 : vertsPerNode + 1));
    gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId + 25] =
      uvec3(baseVertIdx + (faceId < 22 ? 34 : faceId + 20),
	    baseVertIdx + (faceId < 18 ? 33 : faceId < 21 ? faceId + 22 : 34 + vertsPerNode),
	    baseVertIdx + (faceId == 16 ? 18 : faceId < 19 ? 32 : faceId < 24 ? faceId + 21 : vertsPerNode + 18));
  }
}

