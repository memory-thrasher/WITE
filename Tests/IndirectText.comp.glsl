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

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(std140, set = 0, binding = 0) readonly buffer string_t {
  uvec4[64] string;//1024 8-bit characters, null terminated and space padded
} string;

layout(std140, set = 0, binding = 1) readonly buffer charData_t {
  // uvec2[128] charData;//xy = vertCount,firstVirt
  uvec4[64] charData;//xy = vertCount,firstVirt
} charData;

layout(std140, set = 0, binding = 2) buffer indirectBuffer_t {
  //uvec4 just happens to match drawIndirectCommand: xyzw = verts, instances, firstVert, firstInstance
  //instanceid = character position in stream
  uvec4 characterCount;
  uvec4[1024] commands;
} indirectBuffer;

void main() {
  //each execution is a single character, so 1/4 of a uint and 1/16 of a uvec4
  const uint uvid = gl_GlobalInvocationID.x/16,
    compId = (gl_GlobalInvocationID.x/4)%4,
    byteId = gl_GlobalInvocationID.x%4;
  uint c = (string.string[uvid][compId] >> (byteId*8)) & 0xFF;
  if(c == 0)
    indirectBuffer.characterCount.x = gl_GlobalInvocationID.x;
  indirectBuffer.commands[gl_GlobalInvocationID.x] = (c > 32) ?//32 = space
    uvec4(charData.charData[c>>1][(c&1)*2+1], 1, charData.charData[c>>1][(c&1)*2+0], gl_GlobalInvocationID.x) :
    uvec4(0, 0, 0, 0);
}
