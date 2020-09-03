#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout (std140, binding = 1) uniform buf {
		mat4 obj;
} ubuf;
layout (location = 0) out vec4 outColor;
layout (location = 0) in vec3 normal;
layout (location = 1) in vec3 vcolor;
void main() {
   vec3 lightLocation = (ubuf.obj * vec4(2, 3, 5, 1)).xyz;
   vec3 lightDelta = lightLocation / length(lightLocation);
   float lightLevel = abs(dot(lightDelta, normal));
   //outColor = vec4(lightLevel, lightLevel, lightLevel, 1);
   outColor = vec4(lightLevel * vcolor, 1);
}
