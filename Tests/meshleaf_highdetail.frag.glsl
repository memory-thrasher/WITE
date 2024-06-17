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

// layout(location = 0) noperspective in vec4 in_data;

layout(location = 0) out vec4 outColor;

void main() {
  outColor = vec4(0, 1, 0, 1);
  // outColor = in_data.w < 0.1f ? vec4(1, 1, 1, 1) : vec4(0, 1, 0, 1);
  //debug: bumpmap output in world space
  // outColor = in_data * 0.5f + vec4(0.5f, 0.5f, 0.5f, 0.5f);
}
