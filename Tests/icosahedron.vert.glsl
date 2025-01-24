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

layout (std140, set = 0, binding = 0) uniform rendbuf {
  vec4 locr;
} source;

const vec2 planeCorner = normalize(vec2(1, (1+sqrt(5))/2));

void main() {
  int face = gl_VertexIndex / 3, vert = gl_VertexIndex % 3;
  int plane, corner;
  if(face < 8) {
    plane = (((face & 1) ^ ((face >> 1) & 1) ^ ((face >> 2) & 1) ^ (vert & 1)) + 1);//plane = 0|1|2 for x,y,z; corner is bitmask xy, 1 meaning positive
    if(vert == 0) plane = 0;
    //plane = 0|1|2 for x,y,z; corner is bitmask xy, 1 meaning positive
    corner = (((face >> ((plane + 1) % 3)) & 1) << 1) | ((face >> ((plane + 2) % 3)) & 1);
  } else {
    face -= 8;
    const int faceMajorPlane = face >> 2, facePositivity = face & 3, faceMinorPlane = (faceMajorPlane + 1) % 3;
    if((vert & 1) != 0) {
      plane = faceMinorPlane;
      corner = facePositivity;
    } else {
      plane = faceMajorPlane;
      corner = ((((vert >> 1) ^ (facePositivity >> 1) ^ facePositivity) & 1) << 1) | ((facePositivity >> 1) & 1);
    }
  }
  const vec3 base = vec3(planeCorner.y * ((corner & 1) * 2 - 1), planeCorner.x * ((corner >> 1) * 2 - 1), 0);
  const vec3 pnt = vec3(base[plane], base[(plane+1)%3], base[(plane+2)%3]);
  gl_Position = vec4(pnt * source.locr.w + source.locr.xyz, 1);
  //temp: projection
  // const vec3 campos = (targetTrans.transform * gl_Position).xyz;
  // gl_Position = vec4(campos.xy * target.clip.wz * vec2(-1, 1) / campos.z, (-campos.z - target.clip.x) / (target.clip.y - target.clip.x), 1);
}

/*

plane idx to major axis: 0:X  1:Z  2:Y
corner bitmask: 1 = major axis is positive, 2 = minor axis is positive, 3 = both are positive, 0 = both are negative

plane to minor axis to shift of face idx to extract minor axis in lsb position:
0 -> y -> 1
1 -> x -> 2
2 -> z -> 0
(i + 1) % 3

plane to major axis to shift of face idx to extract major axis in lsb position:
0 -> x -> 2
1 -> z -> 0
2 -> y -> 1
(i + 2) % 3

faces 8-19 are those with two points on the same plane, on the major-axis end and one point on an adjacent
(face id below is global face id - 8)
face id bits (0 is lsb):
[0] = minor plane coordinate positive
[1] = major plane coordinate positive
[2:3] = major plane selection (0|1|2)
vertex order of major plane switched to dictate winding order
vertex id: 0->major plane A, 1->minor plane, 2->major plane B
face vert list:
0: (0, 1), (1, 0), (0, 0)
1: (0, 0), (1, 1), (0, 1)

faces 0-7 (verts 0-23) are those containing one point on each axial plane.
face id is a bitmask for which axis contain positive values x=4 y=2 z=1
faces into points in terms of planer and corner
0: (0, 0), (1, 0), (2, 0)
1: (0, 0), (2, 2), (1, 1)
2: (0, 2), (2, 1), (1, 0)
3: (0, 2), (1, 1), (2, 3)
4: (0, 1), (2, 0), (1, 2)
5: (0, 1), (1, 3), (2, 2)
6: (0, 3), (1, 2), (2, 1)
7: (0, 3), (2, 3), (1, 3)





*/

