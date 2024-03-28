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
  outColor = texture(cube, sphereNormWorld) * 0.8f + vec4(0.1, 0.1, 0.1, 0);
  gl_FragDepth = (z*norm.z - near) / (far - near);
}
