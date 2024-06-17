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

layout(std140, set = 0, binding = 0) uniform drawdata_t {
  vec4 color;
  vec4 bbox;//xy = upper-left, zw = width,height; all in normalized screen coords
  vec4 charMetric;//xy = size of each character in normalized screen coords; zw = xy origin within that space
} drawData;

layout(location = 0) in uvec2 coords;

void main() {
  const float widthChars = floor(drawData.bbox.z/drawData.charMetric.x);
  const uvec2 charPos = uvec2(mod(gl_InstanceIndex, widthChars), gl_InstanceIndex / widthChars);
  //TODO truncate at height? in compute?
  gl_Position = vec4((drawData.bbox.xy + drawData.charMetric.xy * (charPos + drawData.charMetric.zw + coords * 0.0714f)) * 2 - vec2(1, 1), 0, 1);
}
