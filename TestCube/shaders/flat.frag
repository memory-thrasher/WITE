#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout (std140, binding = 1) uniform buf {
		mat4 obj;
} ubuf;
layout (location = 0) out vec4 outColor;
layout (location = 0) in vec3 normal;
layout (location = 1) in vec3 worldPos;
layout (location = 2) in vec3 localPos;
void main() {
   vec3 lightLocation = (ubuf.obj  * vec4(1, 0, 0, 1)).xyz - localPos;
   vec3 lightDelta = lightLocation / dot(lightLocation, lightLocation);
   float lightLevel = max(dot(lightDelta, normal), 0);
   outColor = vec4(vec3(1, 1, 1) * lightLevel, 1);
}
