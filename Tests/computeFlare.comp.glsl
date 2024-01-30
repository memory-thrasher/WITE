#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_shader_subgroup_shuffle_relative : enable

layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;

layout(rgba8, set = 0, binding = 0) uniform image2D color;

layout(rgba8, set = 0, binding = 1) uniform image2D temp;

void main() {
  const ivec2 size = imageSize(color);
  const uint tid = uint(gl_GlobalInvocationID.x);//gl_LocalInvocationID.y is always 0
  const uint maxFlare = 16, bufferSize = maxFlare * 2 + 1;
  vec3[bufferSize] colors;
  for(uint y = 0;y < bufferSize;y++) {
    colors[y] = vec3(0, 0, 0);
  }

  if(tid < size.x) {
    uint y = 0;
    for(;y < maxFlare;y++) {
      colors[y] = imageLoad(color, ivec2(tid, y)).xyz;
    }
    for(;y < size.y + maxFlare;y++) {
      colors[y%bufferSize] = y >= size.y ? (0.0f).xxx : imageLoad(color, ivec2(tid, y)).xyz;
      const uint targetY = y - maxFlare;
      vec3 pxl = colors[targetY%bufferSize];
      for(uint i = 1;i <= maxFlare;i++) {
	const float den = 1.0f/(i*i + 1);
	pxl += clamp(colors[(             targetY + i) % bufferSize] - (1.0f).xxx, (0.0f).xxx, (4.0f).xxx) * den;
	pxl += clamp(colors[(bufferSize + targetY - i) % bufferSize] - (1.0f).xxx, (0.0f).xxx, (4.0f).xxx) * den;
      }
      imageStore(temp, ivec2(tid, targetY), vec4(pxl, 1));
    }
  }
}
