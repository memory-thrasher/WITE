#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, set = 0, binding = 1) uniform drawdata_t {
  vec4 bbox;//xy = upper-left, zw = width,height; all in normalized screen coords
  vec4 charMetric;//xy = size of each character in normalized screen coords; zw = xy origin within that space
} drawData;

layout(location = 0) in uvec2 coods;

void main() {
  const float widthChars = floor(drawData.bbox.z/drawData.bbox.x);
  const uvec2 charPos = uvec2(mod(gl_InstanceID, widthChars), gl_InstanceID / widthChars);
  //TODO truncate at height? in compute?
  gl_Position = vec3(charPos * drawData.charMatric.xy + drawData.charMetric.zw + coords * vec2(0.04, 0.04) * drawData.bbox.zw);
}
