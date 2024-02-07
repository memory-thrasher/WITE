#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (std140, set = 0, binding = 0) uniform source_t {
  vec4 loc;//xyz = location, w = radius
} source;

//NOTE: each element is rounded up to vec4 size (16 bytes) so do not use vec3! See https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)
layout(std140, set = 1, binding = 0) uniform target_t {
  vec4 loc, norm, right, up;
  vec4 size;//x, y is size of window, z is cot(fov/2), w is z/aspect
  mat4 transform;
} target;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;
layout(depth_any) out float gl_FragDepth;

void main() {
  const vec3 camLocalNorm = normalize(vec3(inUV.xy / target.size.wz, 1));
  const vec3 norm = (camLocalNorm.x * target.right.xyz +
		     camLocalNorm.y * target.up.xyz +
		     camLocalNorm.z * target.norm.xyz);
  const float near = 0.1f, far = 100, d = far/(far-near);
  const vec3 o = source.loc.xyz - target.loc.xyz;
  const float rayToCenter = dot(normalize(cross(cross(norm, o), norm)), o);
  const float r = source.loc.w;
  if(rayToCenter <= r) {
    //hit, now just find where
    const float sagitta = r - rayToCenter;
    const float dist = dot(o, norm) - sqrt(2*r*sagitta-sagitta*sagitta);
    const float z = dist * dot(target.norm.xyz, norm);
    if(z > near && z < far) {
      outColor = (((norm.y * dist + target.loc.y - source.loc.y) / source.loc.w * 0.25f) + 0.25f) * vec4(0, 0.5, 1, 1);
      gl_FragDepth = pow((d*(z-near)+z)/(2*z), 2);
      // gl_FragDepth = (z - near) / (far - near);
      // outColor = vec4(gl_FragDepth, dist-12, 0, 0);
      return;
    }
  }
  discard;
}

/*
reverse engineering the classic MVPC projection pattern to calculate compatible projection rays and depth for the found fragment

ar = aspect ratio
f = far
n = near
h = cot(fov/2)
w = h*ar
d = f/(f-n) = 1.001
d*-n = -0.1001

projection:

w 0 0 0
0 h 0 0
0 0 d d*-n
0 0 1 0

x *= w
y *= h
z = z*d+w*d*-n
w = z

clip:

1  0  0  0
0 -1  0  0
0  0  ½  ½
0  0  0  1

x = x
y = -y
z = z/2 + w/2
w = w

effective output of projection*clip in terms of input vector x, y, z, 1:

x = w*x/z
y = -h*y/z
z = ((z*d+d*-n)/2 + z/2)/z = (d*(z-n)+z)/(2*z)
w = z

test cases using simple cubes, harvesting the computed depth from gl_FragCoord.z:

cube:
near: 0.1
far: 100
z: 6.9282
depth: 0.9866

calculated:
d = 1.001
depth = (6.9282 * 1.001 - 0.1001 + 6.9282) / (2 * 6.9282)
 = 0.9933

>>>>  0.9933^2 = 0.9866


cube 2:
center: 0,0,0
close face z: -1
real distance: 16
pipeline computed depth map value: 0.9948
computed using RE'd alg: 0.9974
0.9974 = 0.9948^2

>>> undocumented square <<<

*/

