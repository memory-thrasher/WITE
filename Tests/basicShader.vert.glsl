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

layout(std140, set = 1, binding = 0) uniform target_t {
  vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
} target;

layout(std140, set = 1, binding = 1) uniform tt_t {
  mat4 transform;
} targetTrans;

layout (std140, set = 0, binding = 0) uniform rendbuf {
  mat4 trans;
} source;

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normIn;

layout (location = 1) out vec3 norm;
layout (location = 2) out vec3 worldPos;

void main() {
  worldPos = vec3((source.trans * vec4(pos, 1)).xyz);
  vec3 camPos = (targetTrans.transform * vec4(worldPos, 1)).xyz;
  gl_Position = vec4(camPos.xy * target.clip.wz * vec2(-1, 1) / camPos.z,
		     (-camPos.z - target.clip.x) / (target.clip.y - target.clip.x), 1);
  norm = (source.trans * vec4(normIn, 0)).xyz;
}


