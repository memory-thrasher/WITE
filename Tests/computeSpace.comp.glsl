#version 450
#extension GL_ARB_separate_shader_objects : enable

//NOTE: each element is rounded up to vec4 size (16 bytes) so do not use vec3! See https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)
layout (std140, set = 0, binding = 0) uniform target_t {
  vec4 loc, norm, up, right;//w component unused, needed to pad
  vec4 size;//x, y is size of window, z is size of a pixel in radians, w is unused
} target;
layout (rgba8, set = 0, binding = 1) uniform image2D color;

void main() {
  // vec2 pxl = gl_WorkGroupID.xy - target.size.xy * 0.5f;
  // vec2 angleFromCenter = pxl * target.size.z;
  // angleFromCenter.y *= -1;
  // vec3 norm = target.norm.xyz * cos(angleFromCenter.y) + target.up.xyz * sin(angleFromCenter.y);
  // vec3 up = cross(norm, target.right.xyz);
  // norm = norm * cos(angleFromCenter.x) + target.right.xyz * sin(angleFromCenter.x);
  // vec3 right = cross(up, norm);
  // //TODO turn all that ^^^ into an include?
  // const float near = 0.01f, far = 1000;
  // //depth image holds normalized data, but we need to compare it, so denorm
  // float oldDepth = imageLoad(depth, ivec2(gl_WorkGroupID.xy)).x;
  // oldDepth = oldDepth * far + (1-oldDepth) * near;
  // float origDepth = oldDepth;
  // float tint = 0;
  // for(uint i = 0;i < 256;i++) {
  //   vec3 o = source.loc[i].xyz - target.loc.xyz;
  //   vec3 projUp = cross(norm, o);
  //   vec3 projRight = cross(projUp, norm);
  //   float rayToCenter = dot(normalize(projRight), o);
  //   if(rayToCenter <= source.loc[i].w) {
  //     //hit, now just find where
  //     float cordCenterToContact = sqrt((source.loc[i].w - rayToCenter) * (source.loc[i].w + rayToCenter));
  //     float dist = dot(o, norm) - cordCenterToContact;
  //     if(dist > near && dist < oldDepth) {
  // 	vec3 pointOfContact = norm * dist + target.loc.xyz;
  // 	vec3 fragmentNormal = (pointOfContact - source.loc[i].xyz) / source.loc[i].w;
  // 	tint = (fragmentNormal.y * 0.25f) + 0.25f;
  // 	oldDepth = dist;
  //     }
  //   }
  // }
  const double modulus = 307, premodulus = 101, midmod = modulus*premodulus;
  dvec4 l;
  l.x = modf(double((gl_WorkGroupID.x + 1000000) / premodulus), l.z) * premodulus + premodulus;
  l.y = modf(double((gl_WorkGroupID.y + 1000000) / premodulus), l.w) * premodulus + premodulus;
  dvec2 dv = l.xy*l.xy;
  float value = float(floor((mod(l.z*midmod/dv.x + l.w*midmod/dv.y + midmod, modulus))) / modulus);
  value = value >= 0.995f ? (value - 0.99f) * 100 : 0;
  imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(value.xxx, 1));
}
