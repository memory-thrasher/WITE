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
#extension GL_KHR_shader_subgroup_shuffle_relative : enable

layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D color;

layout(rgba8, set = 0, binding = 1) uniform image2D temp;

void main() {
  const ivec2 size = textureSize(color, 0);
  const uint tid = uint(gl_GlobalInvocationID.x);//gl_LocalInvocationID.y is always 0
  const uint maxFlare = 16, bufferSize = maxFlare * 2 + 1;
  const vec2 stride = 1.0f/size;
  vec3[bufferSize] colors;
  for(uint y = 0;y < bufferSize;y++) {
    colors[y] = vec3(0, 0, 0);
  }

  if(tid < size.x) {
    uint y = 0;
    for(;y < maxFlare;y++) {
      colors[y] = texture(color, ivec2(tid, y) * stride).xyz;
    }
    for(;y < size.y + maxFlare;y++) {
      colors[y%bufferSize] = y >= size.y ? (0.0f).xxx : texture(color, ivec2(tid, y) * stride).xyz;
      const uint targetY = y - maxFlare;
      vec3 pxl = colors[targetY%bufferSize];
      for(uint i = 1;i <= maxFlare;i++) {
	const float den = 1.0f/(i*i + 1);
	pxl += clamp(colors[(             targetY + i) % bufferSize] - (1.0f).xxx, (0.0f).xxx, (4.0f).xxx) * den;
	pxl += clamp(colors[(bufferSize + targetY - i) % bufferSize] - (1.0f).xxx, (0.0f).xxx, (4.0f).xxx) * den;
      }
      imageStore(temp, ivec2(tid, targetY), vec4(pxl, 1));
    }
  }
}
