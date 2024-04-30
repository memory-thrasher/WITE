#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(std140, set = 0, binding = 0) readonly buffer string_t {
  uvec4[64] string;//1024 8-bit characters, null terminated and space padded
} string;

layout(std140, set = 0, binding = 1) readonly buffer charData_t {
  // uvec2[128] charData;//xy = vertCount,firstVirt
  uvec4[64] charData;//xy = vertCount,firstVirt
} charData;

layout(std140, set = 0, binding = 2) buffer indirectBuffer_t {
  //uvec4 just happens to match drawIndirectCommand: xyzw = verts, instances, firstVert, firstInstance
  //instanceid = character position in stream
  uvec4 characterCount;
  uvec4[1024] commands;
} indirectBuffer;

void main() {
  //each execution is a single character, so 1/4 of a uint and 1/16 of a uvec4
  const uint uvid = gl_GlobalInvocationID.x/16,
    compId = (gl_GlobalInvocationID.x/4)%4,
    byteId = gl_GlobalInvocationID.x%4;
  uint c = (string.string[uvid][compId] >> (byteId*8)) & 0xFF;
  if(c == 0)
    indirectBuffer.characterCount.x = gl_GlobalInvocationID.x;
  indirectBuffer.commands[gl_GlobalInvocationID.x] = (c > 32) ?//32 = space
    uvec4(charData.charData[c>>1][(c&1)*2+1], 1, charData.charData[c>>1][(c&1)*2+0], gl_GlobalInvocationID.x) :
    uvec4(0, 0, 0, 0);
}
