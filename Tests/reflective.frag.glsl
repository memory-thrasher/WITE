#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 1) uniform samplerCube inCube;

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNorm;

void main() {
  outColor = vec4(1, inUV.xy, 1);
}

