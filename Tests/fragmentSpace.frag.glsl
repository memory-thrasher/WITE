/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#version 450
#extension GL_ARB_separate_shader_objects : enable

//NOTE: each element is rounded up to vec4 size (16 bytes) so do not use vec3! See https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)
layout(std140, set = 1, binding = 0) uniform target_t {
  vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
  ivec4 gridOrigin;
  vec4 geometry;
} target;

layout(std140, set = 1, binding = 1) uniform tt_t {
  mat4 transform;
} targetTrans;

const uint planeCount = 8;
const uint starTypes = 31;

layout(std140, set = 0, binding = 0) readonly buffer scatterData_t {
  ivec4[planeCount] planeIncrement;//w unused
  ivec4[planeCount] planeOffset;
  vec4[starTypes] starTypeColors;
} scatterData;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() {
  const vec3 camLocalNorm = normalize(vec3(inUV.xy / target.clip.wz, 1));
  const vec3 norm = (vec4(camLocalNorm, 0) * targetTrans.transform).xyz * vec3(1, -1, -1);

  const ivec3 rayOrigin = target.gridOrigin.xyz;
  const float maxRange = 2*target.geometry.z/target.geometry.y;
  bool found = false;
  uint starType = 0;
  float depth = maxRange;
  float depthTemp;
  for(uint planeIdx = 0;planeIdx < planeCount;planeIdx++) {
    const uint modulus = (1<<22);
    const ivec3 pi = scatterData.planeIncrement[planeIdx].xyz;
    const ivec3 po = scatterData.planeOffset[planeIdx].xyz;
    const vec3 pn = normalize(vec3(float(pi.x)/pi.z, float(pi.y)/pi.z, 1));
    const float planeDistanceAlongRay = abs(modulus / (dot(pn, pi)*dot(pn, norm)));
    const float rol = mod((dot(po, pi) - dot(rayOrigin, pi)) / dot(norm, pi), planeDistanceAlongRay);
    uint i = 0;
    while((depthTemp = rol + planeDistanceAlongRay*i) < depth) {
      //TODO camera position within sector
      const ivec3 intDelta = ivec3(depthTemp*norm + sign(norm) * 0.5f);
      const ivec3 hit = rayOrigin + intDelta - po;
      if(mod(hit.x * pi.x + hit.y * pi.y + hit.z * pi.z, modulus) == 0) {//TODO prevent overflow, to allow non-pow2 modulus?
	//grid point is on-plane, do the expensive tests
	const uint tempStarType = (hit.x*hit.x + hit.x*7 + hit.y*hit.y*3 + hit.y*13 + hit.z*hit.z*5 + hit.z*37) & 0xFF;
	//< T logic encourages clusters with gaps between
	if(tempStarType < starTypes) {
	  //there is actually a star there. Now see how close the ray is to it (TODO star radius based on type)
	  //inflate radius so it is always at least 2 pixels wide at the given distance
	  const float starRadius = max(pow(10, -7), 4*depthTemp*target.geometry.y);//in lightyears
	  const vec3 floatDelta = intDelta + (0.5f).xxx;
	  const vec3 rayMissVector = dot(floatDelta, norm) * norm - floatDelta;
	  const float sqrDistanceTo = dot(rayMissVector, rayMissVector);
	  if(sqrDistanceTo <= starRadius*starRadius) {
	    starType = tempStarType;
	    depth = depthTemp;
	    found = true;
	  }
	}
      }
      i++;
    }
  }

  if(!found) {
    outColor = vec4(0, 0, 0, 1);
  } else {
    vec3 foundColor = scatterData.starTypeColors[starType].xyz * scatterData.starTypeColors[starType].w * pow(500, 2) / (depth*depth);
    // float mx = max(foundColor.x, max(foundColor.y, foundColor.z));
    // if(mx > 1) foundColor /= mx;
    outColor = vec4(foundColor, 1);
  }

}
