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

layout(triangles) in;
layout(triangle_strip, max_vertices=36) out;
layout(location=1) out vec3 norm;
layout(location=2) out vec3 worldPos;

layout (std140, set = 0, binding = 0) uniform rendbuf {
  vec4 locr;
} source;

layout(std140, set = 1, binding = 0) uniform target_t {
  vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
} target;

layout(std140, set = 1, binding = 1) uniform tt_t {
  mat4 transform;
} targetTrans;

void emit(vec3 world) {
  worldPos = world;
  norm = normalize(world - source.locr.xyz);
  const vec3 campos = (targetTrans.transform * vec4(world, 1)).xyz;
  gl_Position = vec4(campos.xy * target.clip.wz * vec2(-1, 1) / campos.z, (-campos.z - target.clip.x) / (target.clip.y - target.clip.x), 1);
  EmitVertex();
}

vec3 moveToShell(vec3 p) {
  return source.locr.xyz + normalize(p - source.locr.xyz) * source.locr.w;
}

vec3 fromBary(vec3 bary) {
  return gl_in[0].gl_Position.xyz * bary.x + gl_in[1].gl_Position.xyz * bary.y + gl_in[2].gl_Position.xyz * bary.z;
}

void emitBary(vec3 bary) {
  emit(moveToShell(fromBary(bary)));
}

/*
equalateral triangle with area of 1/2
b*h/2 = 1/2
h/(b/2) = tan(60) ≈ 1.73
2h^2 = tan(60)
h = sqrt(tan(60)/2) ≈ 0.930604859
b = 1/h ≈ 1.074569932
*/
//(0, 0),  (unitEqualateralSize.x/2, unitEqualateralSize.y),   (unitEqualateralSize.x, 0)
const vec2 unitEqualateralSize = vec2(1/sqrt(tan(radians(60))/2), sqrt(tan(radians(60))/2));
vec3 unitToBary(vec2 unit) {
  return vec3(1 - unitEqualateralSize.y * unit.x - unitEqualateralSize.x * 0.5f * unit.y,
	      unitEqualateralSize.x * unit.y,
	      unitEqualateralSize.y * unit.x - unitEqualateralSize.x * 0.5f * unit.y);
}

vec3 xyToBary(uvec2 xy, float factor) {
  return unitToBary(vec2(xy.x + xy.y * 0.5f, xy.y) * factor * unitEqualateralSize);
}

void main() {
  //we are handed a face of an icosahedron. Subdivide it.
  const int subdivisions = 4,//TODO automatic from camera distance (LoD)
    rows = subdivisions + 1;
  const float factor = 1.0f/rows;
  //we need barycentric coordinates, so extract them from an imaginary equalateral triangle with base along the x axis and an area of 1/2
  for(int row = 0;row < rows;row++) {
    //prewarm the triangle strip with first corner
    emitBary(xyToBary(uvec2(0, row), factor));
    //each inner iteration adds 2 verts.
    for(int col = 0;col < rows - row;col++) {
      //area is 1 so no need to calculate the det
      emitBary(xyToBary(uvec2(col, row + 1), factor));
      emitBary(xyToBary(uvec2(col + 1, row), factor));
    }
    EndPrimitive();
  }
}

