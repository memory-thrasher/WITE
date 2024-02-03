#version 450

layout (location = 0) out vec2 outUV;

/*
(-1,-1)+-----+--*(3,0)
       |     | /
       |     |/
       +-----*(1,1)
       |    /
       |   /
       |  /
       | /
       |/
       *(0,3)
*/

void main() {
  outUV = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1) * 4.0f - (1.0f).xx;
  gl_Position = vec4(outUV, 0.0f, 1.0f);
}
