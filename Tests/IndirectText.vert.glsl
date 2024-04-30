#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, set = 0, binding = 0) uniform drawdata_t {
  vec4 color;
  vec4 bbox;//xy = upper-left, zw = width,height; all in normalized screen coords
  vec4 charMetric;//xy = size of each character in normalized screen coords; zw = xy origin within that space
} drawData;

layout(location = 0) in uvec2 coords;

void main() {
  const float widthChars = floor(drawData.bbox.z/drawData.charMetric.x);
  const uvec2 charPos = uvec2(mod(gl_InstanceIndex, widthChars), gl_InstanceIndex / widthChars);
  //TODO truncate at height? in compute?
  gl_Position = vec4((drawData.bbox.xy + drawData.charMetric.xy * (charPos + drawData.charMetric.zw + coords * 0.0714f)) * 2 - vec2(1, 1), 0, 1);
}
