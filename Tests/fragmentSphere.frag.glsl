#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (std140, set = 0, binding = 0) uniform source_t {
  vec4 loc;//xyz = location, w = radius
} source;

//NOTE: each element is rounded up to vec4 size (16 bytes) so do not use vec3! See https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)
layout(std140, set = 1, binding = 0) uniform target_t {
  vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
  mat4 transform;
} target;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;
layout(depth_any) out float gl_FragDepth;

void main() {
  const vec3 norm = normalize(vec3(inUV.xy / target.clip.wz, 1));//sqrt
  const float near = target.clip.x, far = target.clip.y;
  const vec3 o = (target.transform * vec4(source.loc.xyz, 1)).xyz * vec3(1, -1, -1);
  const float lenQ = dot(norm, o);
  const vec3 q = norm * lenQ;
  const float rayToCenter = length(q-o);//sqrt
  const float r = source.loc.w;
  if(rayToCenter > r) discard;
  const float sagitta = r - rayToCenter;
  const float z = lenQ - sqrt(2*r*sagitta-sagitta*sagitta);//sqrt
  if(z > far || z < near) discard;
  const vec3 sphereNorm = normalize(norm * z - o);//sqrt
  outColor = vec4(0, 0.5, 1, 1) * (sphereNorm.y * 0.25f + 0.5f);
  gl_FragDepth = (z*norm.z - near) / (far - near);
}
