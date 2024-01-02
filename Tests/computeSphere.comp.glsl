#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (std140, set = 0, binding = 0) uniform source_t {
  vec4 loc;//xyz = location, w = radius
} source;

//NOTE: each element is rounded up to vec4 size (16 bytes) so do not use vec3! See https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)
layout (std140, set = 1, binding = 0) uniform target_t {
  vec4 loc, norm, up, right;//w component unused, needed to pad
  vec4 size;//x, y is size of window, z is size of a pixel in radians, w is unused
} target;
layout (r32f, set = 1, binding = 1) uniform image2D depth;
layout (rgba8, set = 1, binding = 2) uniform image2D color;

void main() {
  const float near = 1.0f, far = 0.0f;
  vec2 pxl = gl_WorkGroupID.xy - target.size.xy * 0.5f;
  vec2 angleFromCenter = pxl * target.size.z;
  // vec2 pxlNorm = gl_WorkGroupID.xy * 2.0f / size.xy - vec2(1, 1);//coordinate [0-1], [0-1] relative to screen size
  //^^ suspect vec operator/(vec,vec) might not be what's intended
  // float aspect = size.y / size.x;
  vec3 norm = target.norm.xyz * cos(angleFromCenter.y) + target.up.xyz * sin(angleFromCenter.y);
  vec3 up = cross(norm, target.right.xyz);
  norm = norm * cos(angleFromCenter.x) + target.right.xyz * sin(angleFromCenter.x);
  vec3 right = cross(up, norm);
  //TODO turn all that ^^^ into an include?
  vec3 o = source.loc.xyz - target.loc.xyz;
  vec3 projUp = cross(norm, o);//len = len(o)*cos(n0o)
  vec3 projRight = cross(projUp, norm);//len = len(projUp)
  //NOTE: if cos is needed, it's free: //dot(projRight, o) = len(o)^2*cos(n0o)
  float rayToCenter = dot(normalize(projRight), o);
  if(rayToCenter <= source.loc.w) {
    //hit, now just find where
    // vec3 centerOfCord = dot(o, norm) * norm;
    float cordCenterToContact = sqrt((source.loc.w - rayToCenter) * (source.loc.w + rayToCenter));
    vec3 pointOfContact = norm * (dot(o, norm) - cordCenterToContact) + target.loc.xyz;
    vec3 fragmentNormal = (pointOfContact - source.loc.xyz) / source.loc.w;
    float tint = 1;//fragmentNormal.y * 0.5f + 0.5f;
    imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(tint.xxx, 1));
  }
}
