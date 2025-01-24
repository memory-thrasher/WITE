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

layout (std140, set = 0, binding = 0) uniform source_t {
  vec4 loc;//xyz = location, w = radius
} source;

layout (set = 0, binding = 1) uniform samplerCube cube;

layout(std140, set = 1, binding = 0) uniform target_t {
  vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
} target;

layout(std140, set = 1, binding = 1) uniform tt_t {
  mat4 transform;
} targetTrans;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;
layout(depth_any) out float gl_FragDepth;

void main() {
  const vec3 norm = normalize(vec3(inUV.xy / target.clip.wz, 1));//sqrt
  const float near = target.clip.x, far = target.clip.y;
  const vec3 o = (targetTrans.transform * vec4(source.loc.xyz, 1)).xyz * vec3(1, -1, -1);
  const float lenQ = dot(norm, o);
  const vec3 q = norm * lenQ;
  const float rayToCenter = length(q-o);//sqrt
  const float r = source.loc.w;
  if(rayToCenter > r) discard;
  const float sagitta = r - rayToCenter;
  const float z = lenQ - sqrt(2*r*sagitta-sagitta*sagitta);//sqrt
  if(z > far || z < near) discard;
  const vec3 camPos = z * norm;
  const vec3 sphereNormCamSpace = normalize(camPos - o);//sqrt
  const vec3 sphereNormWorld = (vec4(sphereNormCamSpace, 0) * targetTrans.transform).xyz;
  // outColor = vec4(sphereNormWorld, 1) * 0.5f + 0.5f;
  // outColor = vec4(texture(cube, sphereNormWorld).xyz, 1) * 0.5f + 0.5f;
  outColor = texture(cube, sphereNormWorld) * 0.8f + vec4(0.1, 0.1, 0.1, 0);
  gl_FragDepth = (z*norm.z - near) / (far - near);
}
