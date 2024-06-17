/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#version 450
#extension GL_EXT_mesh_shader : enable

//actual constants
const float PI = 3.1415926535897932384626f;
const float slopeControl = 30;
const vec3 postScale = 1/vec3(100, 1, 4.4);
const uint vertsPerNode = 15, trisPerNode = 17, maxNodesPerSide = 7;

//x = node position along leaf axis, y = face id within that node
layout(local_size_x = maxNodesPerSide, local_size_y = 2, local_size_z = 1) in;
layout(triangles, max_vertices=maxNodesPerSide*vertsPerNode*2, max_primitives=maxNodesPerSide*trisPerNode*2) out;

layout (std140, set = 0, binding = 0) uniform sharedData {
  mat4 trans;
} source;

layout(std140, set = 1, binding = 0) uniform target_t {
  vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
} target;

layout(std140, set = 1, binding = 1) uniform tt_t {
  mat4 transform;
} targetTrans;

layout(location = 0) out vec4 out_leafLocalPos[];//z unused
layout(location = 1) out perprimitiveEXT uvec4 out_perTri[];//xy = invocation_id.xy, zw = unused

//TODO from input
const uvec4 leafRandomSeed = uvec4(7, 20, 73, 4);
const float detail = 500;//ideal number of verts (min and max apply) TODO from input
const float growthAmount = 1;

vec4 project(vec4 pos) {
  const vec4 worldPos = source.trans * pos;
  const vec3 camPos = (targetTrans.transform * worldPos).xyz;
  return vec4(camPos.xy * target.clip.wz * vec2(-1, 1) / -abs(camPos.z), (-camPos.z - target.clip.x) / (target.clip.y - target.clip.x), 1);
}

vec4 project(vec3 pos) {
  return project(vec4(pos, 1));
}

void putVert(uint idx, vec3 leafPos) {
  if(gl_LocalInvocationID.y == 1)
    leafPos.z *= -1;
  idx += gl_LocalInvocationIndex * vertsPerNode;
  out_leafLocalPos[idx].xyz = leafPos;
  gl_MeshVerticesEXT[idx].gl_Position = project(leafPos);
}

void putTri(uint idx, const uint a, const uint b, const uint c) {
  idx += gl_LocalInvocationIndex * trisPerNode;
  gl_PrimitiveTriangleIndicesEXT[idx] = uvec3(a, b, c) + (gl_LocalInvocationIndex * vertsPerNode).xxx;
  out_perTri[idx] = uvec4(gl_LocalInvocationID.xy, 0, 0);
}

/*
octave:

i=[0:(1/15):15]
x=(i*2*pi+3*(-cos(i*2*pi)).^3)/100
y=(1.5-cos(i*2*pi))*30./(x*100+30)/4.4
ci=[0:0.1:15]
cx=((floor(ci)+0.5)*2*pi+3*(-cos(mod(ci, 1)*pi)).^3)/100
cy=(1.5-cos(mod(ci, 1)*pi))*30./(cx*100+30)/4.4
plot(x, y, cx, cy)

*/

vec3 pointForStep(const uint iter, const float step, const float nodeGrowth, const float nextNodeGrowth) {
  //TODO noise this up
  //lerp node growth from this node's value (at 0) to next node's value (at 1)
  const float cosStep = -cos(step * (2*PI)),
    x = (iter+step)*(2*PI) + 3*pow(cosStep, 3),
    y = (cosStep+1.5f)*slopeControl/(x/mix(nodeGrowth, nextNodeGrowth, step)+slopeControl);
  return vec3(x, 0, y)*postScale;
}

vec3 centerForNormLen(const uint iter, const float step, const float nodeGrowth, const float nextNodeGrowth) {
  const float effectiveGrowth = (nodeGrowth + nextNodeGrowth) * 0.5f,
    cosStep = -cos(step * PI),
    x = (iter+0.5f)*(2*PI) + 3*pow(cosStep, 3),
    y = (cosStep+1.5f)*slopeControl/(x/effectiveGrowth+slopeControl);
  return vec3(x, 0, y)*postScale;
}

//temp: just a starting point, will eventually be more dynamic
void main() {
  const uint nodesPerSide = uint(min(growthAmount * maxNodesPerSide + 1, maxNodesPerSide)), nodes = nodesPerSide*2;
  SetMeshOutputsEXT(vertsPerNode * nodes, trisPerNode * nodes);
  const float thisNodeStart = float(gl_LocalInvocationID.x) / maxNodesPerSide,
    thisNodeGrowth = (growthAmount - thisNodeStart) / (1 - thisNodeStart),
    nextNodeStart = float(gl_LocalInvocationID.x + 1) / maxNodesPerSide,
    nextNodeGrowth = max(0, (growthAmount - nextNodeStart) / (1 - nextNodeStart)),
    depth = 0.01f * thisNodeGrowth;
  const vec3 baseCenter = centerForNormLen(gl_LocalInvocationID.x, 0, thisNodeGrowth, nextNodeGrowth),
    leftCrevice = pointForStep(gl_LocalInvocationID.x, 0, thisNodeGrowth, nextNodeGrowth),
    rightCrevice = pointForStep(gl_LocalInvocationID.x, 1, thisNodeGrowth, nextNodeGrowth),
    leftCreviceToCenter = leftCrevice - baseCenter,
    rightCreviceToCenter = rightCrevice - baseCenter,
    tip = pointForStep(gl_LocalInvocationID.x, 0.5f, thisNodeGrowth, nextNodeGrowth),
    passed = baseCenter + (tip - baseCenter) * 1.05f,
    norm = normalize(baseCenter - tip);
  putVert(1, centerForNormLen(gl_LocalInvocationID.x, 0.95f, thisNodeGrowth, nextNodeGrowth));
  putVert(0, passed + leftCreviceToCenter + vec3(0, depth, 0));
  putVert(2, passed + rightCreviceToCenter + vec3(0, depth, 0));
  putTri(0, 0, 1, 2);
  putVert(3*3 + 3 + 1, baseCenter + norm * dot(norm, -baseCenter));
  putVert(3*3 + 3 + 0, leftCrevice + norm * dot(norm, -leftCrevice));
  putVert(3*3 + 3 + 2, rightCrevice + norm * dot(norm, -rightCrevice));
  putTri(3*4 + 1, 3*3 + 0, 3*3 + 1, 3*3 + 3);
  putTri(3*4 + 2, 3*3 + 4, 3*3 + 1, 3*3 + 3);
  putTri(3*4 + 3, 3*3 + 2, 3*3 + 1, 3*3 + 5);
  putTri(3*4 + 4, 3*3 + 4, 3*3 + 1, 3*3 + 5);
  putVert(3*3 + 3 + 1, (baseCenter) * vec3(1, 1, 0));
  putVert(3*3 + 3 + 0, (baseCenter + leftCreviceToCenter) * vec3(1, 1, 0));
  putVert(3*3 + 3 + 2, (baseCenter + rightCreviceToCenter) * vec3(1, 1, 0));
  for(uint i = 0;i < 3;i++) {
    const vec3 center = centerForNormLen(gl_LocalInvocationID.x, (2-i)/3.0f, thisNodeGrowth, nextNodeGrowth);
    putVert(i*3 + 3 + 1, center);
    putVert(i*3 + 3 + 0, center + leftCreviceToCenter + vec3(0, depth, 0));
    putVert(i*3 + 3 + 2, center + rightCreviceToCenter + vec3(0, depth, 0));
    putTri(i*4 + 1, i*3 + 0, i*3 + 1, i*3 + 3);
    putTri(i*4 + 2, i*3 + 4, i*3 + 1, i*3 + 3);
    putTri(i*4 + 3, i*3 + 2, i*3 + 1, i*3 + 5);
    putTri(i*4 + 4, i*3 + 4, i*3 + 1, i*3 + 5);
  }
}



// layout(local_size_x = 5, local_size_y = 25, local_size_z = 1) in;

// layout(location = 0) out vec4 out_data[];//packed: xyz=normal, w: 0=stem 1=flesh

// const float PI = 3.1415926535897932384626f;
// const float segmentTiltAngle = 20*PI/180;
// const uint vertsPerNode = 45, facesPerNode = 50, finalSegmentVerts = 44, finalSegmentFaces = 47;

// vec4 project(vec4 pos) {
//   const vec4 worldPos = source.trans * pos;
//   const vec3 camPos = (targetTrans.transform * worldPos).xyz;
//   return vec4(camPos.xy * target.clip.wz * vec2(-1, 1) / -abs(camPos.z), (-camPos.z - target.clip.x) / (target.clip.y - target.clip.x), 1);
// }

// vec4 project(vec3 pos) {
//   return project(vec4(pos, 1));
// }

// vec3 projectNormal(vec3 pos) {
//   //normals use world space not camera space
//   return (source.trans * vec4(pos, 0)).xyz;
// }

// float iterOffset(int iter) {
//   return 2.5f*iter;
// }

// float iterOffset(uint iter) {
//   return iterOffset(int(iter));
// }

// vec4 transformFor(vec3 p, int iter) {
//   const float scale = 25.0f/(iter * iter + 25);
//   return mat4(cos(segmentTiltAngle)*sqrt(2)/2*scale, sin(segmentTiltAngle), sqrt(2)/2*scale, 0,
// 	      -sin(segmentTiltAngle)*scale, cos(segmentTiltAngle), 0, 0,
// 	      -sqrt(2)/2*scale, 0, sqrt(2)/2*scale, 0,
// 	      iterOffset(iter), 0, scale*2, 1) * vec4(p, 1);
// }

// vec3 transformNormalFor(vec3 p, int iter) {
//   const float scale = 25.0f/(iter * iter + 25);
//   return (mat4(cos(segmentTiltAngle)*sqrt(2)/2*scale, sin(segmentTiltAngle), sqrt(2)/2*scale, 0,
// 	       -sin(segmentTiltAngle)*scale, cos(segmentTiltAngle), 0, 0,
// 	       -sqrt(2)/2*scale, 0, sqrt(2)/2*scale, 0,
// 	       0, 0, 0, 0) * vec4(p, 0)).xyz;
// }

// vec3 transformNormalFor(vec3 p, uint iter) {
//   return transformNormalFor(p, int(iter));
// }

// vec4 pForFaceId(uint pointId, int iter) {
//   vec3 p;
//   if(pointId == 0)
//     p = vec3(0, -0.25f, 0);
//   else {
//     const uint faceIdForP = pointId - 1;
//     const uint xd = faceIdForP <= 8 ? faceIdForP : 16 - faceIdForP, yd = xd & 7;
//     p = vec3(xd * (3 / 8.0f) - 1, 0.1f * (faceIdForP&1), (sin(yd*(3*PI/16.0f))*0.75f + yd/6.0f) * sign(8-int(faceIdForP)));
//   }
//   return transformFor(p, iter);
// }

// vec4 pForFaceId(uint pointId, uint iter) {
//   return pForFaceId(pointId, int(iter));
// }

// //2857fps (with colorized normal on fragment)
// void main() {
//   SetMeshOutputsEXT(vertsPerNode*4 + finalSegmentVerts, facesPerNode*4 + finalSegmentFaces);
//   const uint faceId = gl_LocalInvocationID.y;
//   const uint baseFaceIdx = gl_LocalInvocationID.x * facesPerNode;
//   const uint baseVertIdx = gl_LocalInvocationID.x * vertsPerNode;
//   const vec4 p = pForFaceId(faceId < 16 ? faceId+1 : 16, gl_LocalInvocationID.x);
//   const vec4 q = pForFaceId(faceId < 16 ? 0 : 2, gl_LocalInvocationID.x + (faceId < 16 ? 0 : 1));
//   if(faceId < 16) {//consumes 16 of every 25 invocations, 34 of every 50 vertices, and 32 of every 50 triangles
//     //each invocation draws two triangles, mirrored across the XZ plane
//     const vec3 cn = transformNormalFor(vec3(0, 1, 0), gl_LocalInvocationID.x);
//     if(faceId == 0) {
//       gl_MeshVerticesEXT[baseVertIdx].gl_Position = project(q);
//       gl_MeshVerticesEXT[baseVertIdx+17].gl_Position = project(q * vec4(1, 1, -1, 1));
//       out_data[baseVertIdx] = vec4(projectNormal(cn), 0);
//       out_data[baseVertIdx+17] = vec4(projectNormal(cn * vec3(1, 1, -1)), 0);
//     }
//     gl_MeshVerticesEXT[baseVertIdx + faceId + 1].gl_Position = project(p);
//     gl_MeshVerticesEXT[baseVertIdx + faceId + 18].gl_Position = project(p * vec4(1, 1, -1, 1));
//     const vec3 d = (p-q).xyz;
//     const vec3 n = normalize(cross(cross(d, cn), d));
//     out_data[baseVertIdx + faceId + 1] = vec4(projectNormal(n), faceId == 0 ? 0 : faceId == 8 ? 0.1f : 1);
//     out_data[baseVertIdx + faceId + 18] = vec4(projectNormal(n * vec3(1, 1, -1)), faceId == 0 ? 0 : faceId == 8 ? 0.1f : 1);
//     gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId] = uvec3(0, faceId == 15 ? 1 : faceId+2, faceId+1) +
//       uvec3(1, 1, 1) * baseVertIdx;
//     gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId + 16] = uvec3(0, faceId == 15 ? 1 : faceId+2, faceId+1) +
//       uvec3(1, 1, 1) * (baseVertIdx + 17);
//   } else {
//     const vec3 bp = vec3(iterOffset(gl_LocalInvocationID.x) - 1.5f, pow(gl_LocalInvocationID.x * 0.2f, 2) - 0.8f, 0);
//     const vec3 cn = normalize(vec3(pow((int(gl_LocalInvocationID.x) - 1) * 0.2f, 2) - pow((gl_LocalInvocationID.x + 1) * 0.2f, 2), iterOffset(gl_LocalInvocationID.x + 1) - iterOffset(int(gl_LocalInvocationID.x) - 1), 0));
//     if(faceId == 16) {
//       gl_MeshVerticesEXT[baseVertIdx + 34].gl_Position = project(bp);
//       out_data[baseVertIdx + 34] = vec4(projectNormal(cn), 0);
//     }
//     if(gl_LocalInvocationID.x == 4) {//endcap
//       if(faceId < 23) {
// 	if(faceId > 16 && faceId < 22) {
// 	  const float y = (21 - faceId) / 5.0f;
// 	  const vec3 interp = p.xyz * vec3(1, 1, y) + vec3(((y*y)-1)*0.5f, (~faceId&1) * 0.05f, 0);
// 	  gl_MeshVerticesEXT[baseVertIdx + faceId + 18].gl_Position = project(interp);
// 	  const vec3 d = interp-bp;
// 	  const vec3 n = normalize(cross(cross(d, cn), d));
// 	  out_data[baseVertIdx + faceId + 18] = vec4(projectNormal(n), 0.5f);
// 	  if(faceId < 24) {
// 	    gl_MeshVerticesEXT[baseVertIdx + faceId + 23].gl_Position = project(interp * vec3(1, 1, -1));
// 	    out_data[baseVertIdx + faceId + 23] = vec4(projectNormal(n * vec3(1, 1, -1)), 0.5f);
// 	  }
// 	}
// 	gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId + 16] =
// 	  uvec3(baseVertIdx + 34,
// 		baseVertIdx + (faceId == 16 ? 2 : faceId < 19 ? 16 : faceId + 16),
// 		baseVertIdx + (faceId < 18 ? 1 : faceId + 17));
// 	gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId + 23] =
// 	  uvec3(baseVertIdx + 34,
// 		baseVertIdx + (faceId == 16 ? 19 : faceId < 19 ? 33 : faceId + 21),
// 		baseVertIdx + (faceId < 18 ? 18 : faceId < 25 ? faceId + 22 : 39));
//       }
//     } else {
//       if(faceId > 16 && faceId < 22) {
// 	const float a = (faceId - 16.0f) / 6;
// 	const vec3 nodeConDelta = q.xyz - p.xyz;
// 	const vec3 interp = mix(p.xyz, q.xyz, a) +
// 	  cross(nodeConDelta, vec3(0, -1, 0)) * (0.25f-pow(a-0.5f, 2)) +
// 	  cross(nodeConDelta, vec3(0, 0, 1)) * ((faceId&1) * 0.05f);
// 	gl_MeshVerticesEXT[baseVertIdx + faceId + 18].gl_Position = project(interp);
// 	gl_MeshVerticesEXT[baseVertIdx + faceId + 23].gl_Position = project(interp * vec3(1, 1, -1));
// 	const vec3 d = interp-bp;
// 	const vec3 n = normalize(cross(cross(d, cn), d));
// 	out_data[baseVertIdx + faceId + 18] = vec4(projectNormal(n), 0.8f);
// 	out_data[baseVertIdx + faceId + 23] = vec4(projectNormal(n * vec3(1, 1, -1)), 0.8f);
//       }
//       gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId + 16] =
// 	uvec3(baseVertIdx + (faceId < 22 ? 34 : faceId + 15),
// 	      baseVertIdx + (faceId < 18 ? 1 : faceId < 21 ? faceId + 17 : 34 + vertsPerNode),
// 	      baseVertIdx + (faceId == 16 ? 2 : faceId < 19 ? 16 : faceId < 24 ? faceId + 16 : vertsPerNode + 2));
//       gl_PrimitiveTriangleIndicesEXT[baseFaceIdx + faceId + 25] =
// 	uvec3(baseVertIdx + (faceId < 22 ? 34 : faceId + 20),
// 	      baseVertIdx + (faceId < 18 ? 18 : faceId < 21 ? faceId + 22 : 34 + vertsPerNode),
// 	      baseVertIdx + (faceId == 16 ? 19 : faceId < 19 ? 33 : faceId < 24 ? faceId + 21 : vertsPerNode + 19));
//     }
//   }
// }

