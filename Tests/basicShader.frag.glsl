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

layout (location = 1) in vec3 norm;
layout (location = 2) in vec3 worldPos;

layout (location = 0) out vec4 outColor;

void main() {
  const vec3 light = { 0, 2, 1 };
  vec3 peakShine = normalize(light - worldPos);
  outColor = (clamp(dot(normalize(norm), peakShine), 0, 1)/2 + 0.5) * vec4(1, 0.5, 0, 1);
  // outColor = vec4(norm, 1);
  // outColor = vec4(1, 1, 1, 1);
}
