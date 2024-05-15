#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) noperspective in vec4 in_data;

layout(location = 0) out vec4 outColor;

void main() {
  // outColor = in_data.w < 0.1f ? vec4(1, 1, 1, 1) : vec4(0, 1, 0, 1);
  //debug: bumpmap output in world space
  outColor = in_data * 0.5f + vec4(0.5f, 0.5f, 0.5f, 0.5f);
}
