#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout (std140, binding = 0) uniform buf {
        mat4 mvp;
} ubuf;
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 color;
layout (location = 0) out vec3 normalOut;
layout (location = 1) out vec3 vcolor;
void main() {
   gl_Position = ubuf.mvp * vec4(pos, 1);
   mat3 mvpDirOnly = mat3(ubuf.mvp[0].xyz, ubuf.mvp[1].xyz, ubuf.mvp[2].xyz);
   normalOut = /*mvpDirOnly */ normal;
   vcolor = color;
}
