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

layout(rgba8, set = 0, binding = 0) uniform image2D color;

layout(set = 0, binding = 1) uniform sampler2D temp;

/*
timing breakdown:
everything but with maxFlare = 4 (both parts): 1:55 (maybe this allowed more parallel workgroups by using fewer registers?)
everything: 2:13 (1.33ms)
without image reads: 1:53 (1.13ms)
without image reads or writes: 1:38 (980us)
empty: 1:34 (940us)
no part2 at all: 1:35 (950us)
image reads: 200us
image writes: 150us
logic: 40us
shader call overhead: 0
*/

//there is a problem somewhere in here that is somehow corrupting the temp image. Not worth fixing because performance isn't great either. Switching to a fragment shader w/ blend paradigm.

void main() {
  const ivec2 size = imageSize(color);
  const uint tid = uint(gl_WorkGroupID.y * 128 + gl_LocalInvocationID.x);//gl_LocalInvocationID.y is always 0
  const uint maxFlare = 16, bufferSize = maxFlare * 2 + 1;
  const vec2 stride = 1.0f/size;
  vec3[bufferSize] colors;
  for(uint x = 0;x < bufferSize;x++) {
    colors[x] = vec3(0, 0, 0);
  }

  if(tid < size.y) {
    uint x = 0;
    for(;x < maxFlare;x++) {
      colors[x] = texture(temp, ivec2(x, tid) * stride).xyz;
    }
    for(;x < size.x + maxFlare;x++) {
      colors[x%bufferSize] = x >= size.x ? (0.0f).xxx : texture(temp, ivec2(x, tid) * stride).xyz;
      const uint targetX = x - maxFlare;
      vec3 pxl = colors[targetX%bufferSize];
      for(uint i = 1;i <= maxFlare;i++) {
	const float den = 1.0f/(i*i + 1);
	pxl += clamp(colors[(             targetX + i) % bufferSize] - (1.0f).xxx, (0.0f).xxx, (4.0f).xxx) * den;
	pxl += clamp(colors[(bufferSize + targetX - i) % bufferSize] - (1.0f).xxx, (0.0f).xxx, (4.0f).xxx) * den;
      }
      imageStore(color, ivec2(targetX, tid), vec4(pxl, 1));
    }
  }
}
