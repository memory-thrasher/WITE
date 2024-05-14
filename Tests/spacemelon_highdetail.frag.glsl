#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) noperspective in float in_type;

layout(location = 0) out vec4 outColor;

void main() {
  outColor = in_type < 0.1f ? vec4(1, 1, 1, 1) : vec4(0, 1, 0, 1);
}
