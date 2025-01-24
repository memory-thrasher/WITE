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

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D inImg;

layout (constant_id = 0) const int vertical = 0;//0 or 1

void main() {
  const vec2 size = textureSize(inImg, 0);
  const uint maxFlare = 4;
  // const uvec2 target = uvec2(inUV * target.size.xy + (0.5f).xx);
  const vec2 pxlStride = vec2((vertical^1)/size.x, vertical/size.y);
  const vec2 uv = (inUV + 1) * 0.5f;
  vec3 pxl = texture(inImg, uv).xyz;
  for(uint i = 1;i <= maxFlare;i++) {
    const float den = 1.0f/(i*i + 1);
    pxl += clamp(texture(inImg, uv + i*pxlStride).xyz - (1.0f).xxx, (0.0f).xxx, (1.0f).xxx) * den;
    pxl += clamp(texture(inImg, uv - i*pxlStride).xyz - (1.0f).xxx, (0.0f).xxx, (1.0f).xxx) * den;
  }
  // float mx = max(max(pxl.x, pxl.y), pxl.z);
  // if(mx > 1) pxl /= mx;
  outColor = vec4(pxl, 1);
}
