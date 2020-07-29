#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout (std140, binding = 0) uniform buf {
        dmat4 mvp;
} ubuf;
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 color;
void main() {
   gl_Position = vec4(ubuf.mvp * dvec4(pos, 1));
}
