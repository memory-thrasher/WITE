#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, set = 0, binding = 0) uniform color_t {
  vec4 color;
} color;

layout(location = 0) out vec4 outColor;

void main() {
  outColor = color.color;
}
