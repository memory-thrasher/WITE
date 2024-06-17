/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#version 450

layout (location = 0) out vec2 outUV;

layout (constant_id = 0) const float depth = 0;//0..1

/*
(-1,-1)+-----+--*(3,0)
       |     | /
       |     |/
       +-----*(1,1)
       |    /
       |   /
       |  /
       | /
       |/
       *(0,3)
*/

void main() {
  outUV = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1) * 4.0f - (1.0f).xx;
  gl_Position = vec4(outUV, depth, 1.0f);
}
