/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(local_size_x = 30, local_size_y = 30, local_size_z = 1) in;

//NOTE: each element is rounded up to vec4 size (16 bytes) so do not use vec3! See https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)
layout (std140, set = 0, binding = 0) uniform target_t {
  vec4 loc, norm, up, right;//w component unused, needed to pad
  vec4 size;//x, y is size of window, z is size of a pixel in radians, w is tan(z)
  ivec4 gridOrigin;//w is render distance in pixels of cubic light year apparent width
} target;

layout (rgba8, set = 0, binding = 1) uniform image2D color;

layout (std140, set = 0, binding = 2) uniform scatterData_t {
  ivec4[48] data;//2 data per plane, {(pi.xyz, m), (po.xyz, unused)}
} scatterData;

const uint starTypes = 31;
const vec3[starTypes] starTypeColors = {
  vec3(0.6f, 0.69f, 1) * 10 * 2,
  vec3(0.57f, 0.71f, 1) * 8 * 2,
  vec3(0.667f, 0.749f, 1) * 6 * 2,
  vec3(0.635f, 0.753f, 1) * 4 * 2,
  vec3(0.792f, 0.843f, 1) * 3 * 2,
  vec3(0.835f, 0.878f, 1) * 2 * 2,
  vec3(0.973f, 0.969f, 1) * 2 * 2,
  vec3(1, 0.957f, 0.918f) * 2,
  vec3(1, 0.929f, 0.89f) * 2,
  vec3(1, 0.824f, 0.631f) * 2,
  vec3(1, 0.855f, 0.71f) * 2,
  vec3(1, 0.8f, 0.435f) * 2,
  vec3(1, 0.71f, 0.424f) * 2,
  vec3(1, 0.947f, 0.908f) * 2,
  vec3(1, 0.859f, 0.76f) * 2,
  vec3(1, 0.834f, 0.671f) * 2,
  vec3(1, 0.825f, 0.661f) * 2,
  vec3(1, 0.76f, 0.43f) * 2,
  vec3(1, 0.61f, 0.344f) * 2,
  vec3(0.792f, 0.843f, 1),
  vec3(0.835f, 0.878f, 1),
  vec3(0.973f, 0.969f, 1),
  vec3(1, 0.957f, 0.918f),
  vec3(1, 0.929f, 0.89f),
  vec3(1, 0.824f, 0.631f),
  vec3(1, 0.855f, 0.71f),
  vec3(1, 0.8f, 0.435f),
  vec3(1, 0.71f, 0.424f),
  vec3(1, 0.947f, 0.908f),
  vec3(1, 0.859f, 0.76f),
  vec3(1, 0.834f, 0.671f)
};

void main() {
  const vec2 pxl = gl_GlobalInvocationID.xy - target.size.xy * 0.5f;
  vec2 angleFromCenter = pxl * target.size.z;
  angleFromCenter.y *= -1;
  vec3 norm = target.norm.xyz * cos(angleFromCenter.y) + target.up.xyz * sin(angleFromCenter.y);
  const vec3 up = cross(norm, target.right.xyz);
  norm = norm * cos(angleFromCenter.x) + target.right.xyz * sin(angleFromCenter.x);
  // vec3 right = cross(up, norm);

  const ivec3 rayOrigin = target.gridOrigin.xyz;
  const float maxRange = target.gridOrigin.w/target.size.w;
  bool found = false;
  uint starType = 0;
  float depth = maxRange;
  float depthTemp;
  for(uint planeIdx = 0;planeIdx < 48;planeIdx++) {
    const uint modulus = scatterData.data[planeIdx*2].w;//TODO non-power-of-2 modulus
    const ivec3 pi = scatterData.data[planeIdx*2].xyz;
    const ivec3 po = scatterData.data[planeIdx*2+1].xyz;
    const vec3 pn = normalize(vec3(float(pi.x)/pi.z, float(pi.y)/pi.z, 1));
    const uint maxIterations = scatterData.data[planeIdx*2+1].w;
    const float planeDistanceAlongRay = abs(modulus / (dot(pn, pi)*dot(pn, norm)));
    const float rol = mod((dot(po, pi) - dot(rayOrigin, pi)) / dot(norm, pi), planeDistanceAlongRay);
    for(uint i = 0;i < maxIterations;i++) {
      depthTemp = rol + planeDistanceAlongRay*i;
      if(depthTemp >= depth)
	break;
      const vec3 proj = depthTemp*norm;
      const ivec3 hit = rayOrigin + ivec3(proj + sign(norm) * 0.5f) - po;
      if(mod((hit.x * pi.x + hit.y * pi.y + hit.z * pi.z), modulus) == 0) {
	//if the star is so close that the grid point can hit multiple pixels consider the size of the star within the grid cube
	const vec3 nearPointToCore = mod(proj, 1);
	const float pixelSizeLy = abs(depthTemp) * target.size.w * 3;//extra *2 to prefer large stars to missing stars when float precision issues happen
	if(pixelSizeLy >= 0.5f || dot(nearPointToCore, nearPointToCore) <= pow(max(pixelSizeLy, pow(10, -7)), 2)) {
	  //grid point is on-plane, do the expensive test
	  const uint tempStarType = (hit.x*hit.x + hit.x*7 + hit.y*hit.y*3 + hit.y*13 + hit.z*hit.z*5 + hit.z*37) & 0xFF;
	  //< T logic encourages clusters with gaps between
	  if(tempStarType < starTypes) {
	    starType = tempStarType;
	    depth = depthTemp;
	    found = true;
	  }
	}
      }
    }
  }

  if(!found) {
    imageStore(color, ivec2(gl_GlobalInvocationID.xy), vec4(0, 0, 0, 1));
  } else {
    vec3 foundColor = starTypeColors[starType] * pow(500, 2) / (depth*depth);
    // float mx = max(foundColor.x, max(foundColor.y, foundColor.z));
    // if(mx > 1) foundColor /= mx;
    imageStore(color, ivec2(gl_GlobalInvocationID.xy), vec4(foundColor, 1));
  }

}
