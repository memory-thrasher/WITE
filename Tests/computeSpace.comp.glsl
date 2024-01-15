#version 450
#extension GL_ARB_separate_shader_objects : enable

// const uvec3[8] pByBlock = { uvec3(15131, 15583, 16067), uvec3(19121, 41213, 24977), uvec3(38963, 32279, 56139), uvec3(13561, 24485, 27625), uvec3(12299, 35599, 19091), uvec3(42881, 39309, 28801), uvec3(59555, 20935, 24219), uvec3(5193, 55477, 38233) };

// const ivec3 pi = ivec3(1, 1, 1), po = ivec3(0, 0, 2);
// const ivec3 pi = ivec3(5, 17, 31), po = ivec3(0, 0, 2);
//const vec3 pi = vec3(5.47, 4.19, 0.283), po = vec3(1.51313, 1.5583, 1.6067);
const ivec3 pi = ivec3(547, 419, 283), po = ivec3(151313, 15583, 16067);
const int modulus = 1<<18, modulusMask = modulus - 1;
const vec3 pn = normalize(vec3(float(pi.x)/pi.z, float(pi.y)/pi.z, 1));
// const vec3 colors[passValues] = { vec3(1, 1, 1), vec3(1, 0.5f, 0.4f), vec3(0.5, 0.6, 1) };
const float pd = modulus/dot(pn, pi), po2 = dot(po, pi);

//NOTE: each element is rounded up to vec4 size (16 bytes) so do not use vec3! See https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)
layout (std140, set = 0, binding = 0) uniform target_t {
  vec4 loc, norm, up, right;//w component unused, needed to pad
  vec4 size;//x, y is size of window, z is size of a pixel in radians, w is tan(z)
  ivec4 gridOrigin;
} target;

layout (rgba8, set = 0, binding = 1) uniform image2D color;

// layout (std140, set = 0, binding = 2) readonly buffer scatterData_t {
//   uvec4[maxDepth * modulus / 4] data;
// } scatterData;

// //Stein's algorithm
// uint gcd(uint m, uint n) {
//   int mk = findLSB(m),
//     k = min(mk, findLSB(n));
//   m >>= mk;
//   while(n != 0) {
//     n >>= findLSB(n);
//     if(m > n) {
//       uint t = n;
//       n = m;
//       m = t;
//     }
//     n -= m;
//   }
//   return m << k;
// }

// uint lcm(uint m, uint n) {
//   if(m == 0 || n == 0) return 0;
//   return m * (n / gcd(m, n));
// }

// uint lowestIntMultiple(float x) {
//   const uint sensitivity = 10000;
//   return sensitivity / gcd(sensitivity, uint(sensitivity * abs(x)));
// }

// uint lowestIntMultiple(float x) {
//   const uint maxIterations = 10;
//   const float epsilon = 0.00001;
//   uint i = 0;
//   float[maxIterations+1] layers;
//   layers[0] = abs(x);
//   for(i = 0;i < maxIterations;++i) {
//     if(mod(layers[i] + epsilon, 1) < 2*epsilon)
//       break;
//     layers[i+1] = 1/mod(layers[i], 1);
//   }
//   float ret = 1;
//   for(;i > 0;--i) {//skip layer 0; we don't want x in the output
//     ret *= layers[i];
//   }
//   return uint(0.5f+ret);
// }

void main() {
  vec2 pxl = gl_WorkGroupID.xy - target.size.xy * 0.5f;
  vec2 angleFromCenter = pxl * target.size.z;
  angleFromCenter.y *= -1;
  vec3 norm = target.norm.xyz * cos(angleFromCenter.y) + target.up.xyz * sin(angleFromCenter.y);
  vec3 up = cross(norm, target.right.xyz);
  norm = norm * cos(angleFromCenter.x) + target.right.xyz * sin(angleFromCenter.x);
  // vec3 right = cross(up, norm);

  float planeDistanceAlongRay = abs(pd / dot(pn, norm));
  ivec3 rayOrigin = target.gridOrigin.xyz;
  float max_range = 1/target.size.w;
  uint max_iterations = target.gridOrigin.w;
  bool found = false;
  float depth = max_range;
  float depthTemp;
  float rol = mod((po2 - dot(rayOrigin, pi)) / dot(norm, pi), planeDistanceAlongRay);

  if(rol < 0) {
    imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(1, 0, 0, 1));
    return;
  }

  for(uint i = 0;i < max_iterations;i++) {
    depthTemp = rol + planeDistanceAlongRay*i;
    if(depthTemp >= depth)
      break;
    ivec3 hit = rayOrigin + ivec3(depthTemp*norm + sign(norm) * 0.5f) - po;
    if(((hit.x * pi.x + hit.y * pi.y + hit.z * pi.z) & modulusMask) == 0) {
      depth = depthTemp;
      found = true;
    }
  }

  // found = rol < max_range;
  // depth = rol;

  if(!found) {
    imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(0, 0, 0, 1));
  } else {
    vec3 foundColor = vec3(1, 1, 1) * pow(2, 30) / (depth*depth);
    // vec3 foundColor = vec3(1, 1, 1) * planeDistanceAlongRay / pow(2, 20);
    // vec3 foundColor = vec3(1, 1, 1) * depth / float(1<<6);
    // vec3 foundColor = norm / 2 + vec3(1, 1, 1) / 2;
    // uvec3 location = rayOrigin + uvec3(norm*depth + vec3(0.5f, 0.5f, 0.5f));
    foundColor = max(min(foundColor, vec3(1, 1, 1)), vec3(0, 0, 0));
    // if(max(max(foundColor.x, foundColor.y), foundColor.z) > 1) foundColor = normalize(foundColor);
    imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(foundColor, 1));
  }


  // float planeDistanceAlongRay = (pd / dot(pn, norm));
  // uvec3 rayOrigin = target.gridOrigin.xyz;
  // // float pixel_tangent = target.size.w;
  // // float max_range = 1 << 16;//pixel_tangent <= (1.0f/(1<<16)) ? 1<<16 : 1/pixel_tangent;
  // // uint rm = lcm(lcm(lowestIntMultiple(norm.x), lowestIntMultiple(norm.y)),
  // // 		lcm(lowestIntMultiple(norm.z), lowestIntMultiple(planeDistanceAlongRay)));
  // //TODO edge case, any element of norm is 0
  // const float max_range = pow(2, 30);
  // // float depth = max_range;
  // float depth = abs(mod((po2 - dot(rayOrigin, pi)) / dot(norm, pi), planeDistanceAlongRay));
  // if(depth >= max_range) {
  //   imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(0, 0, 0, 1));
  // } else {
  //   vec3 foundColor = vec3(1, 1, 1) * pow(2, 4) / (depth*depth);
  //   // vec3 foundColor = vec3(1, 1, 1) * planeDistanceAlongRay / pow(2, 20);
  //   // vec3 foundColor = vec3(1, 1, 1) * depth / float(1<<6);
  //   // vec3 foundColor = norm / 2 + vec3(1, 1, 1) / 2;
  //   // uvec3 location = rayOrigin + uvec3(norm*depth + vec3(0.5f, 0.5f, 0.5f));
  //   foundColor = max(min(foundColor, vec3(1, 1, 1)), vec3(0, 0, 0));
  //   // if(max(max(foundColor.x, foundColor.y), foundColor.z) > 1) foundColor = normalize(foundColor);
  //   imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(foundColor, 1));
  // }

  // //35
  // vec3 forwardDelta = abs(norm);
  // forwardDelta = norm / max(max(forwardDelta.x, forwardDelta.y), forwardDelta.z);
  // uint slopeNum = uint(abs(forwardDelta.x * forwardDelta.y * forwardDelta.z) * 1000000 + 0.5f);
  // uint slopeDen = 1000000;
  // //num is the numerator and den is the denominator of the slope of the ray. Simplify the fraction by LCM with the known prime factors of 1000000: 2^6*5^6
  // for(uint i = 0;i < 6;i++) {
  //   if((slopeNum & 1) == 0) {
  //     slopeNum >>= 1;
  //     slopeDen >>= 1;
  //   }
  //   if(slopeNum % 5 == 0) {
  //     slopeNum /= 5;
  //     slopeDen /= 5;
  //   }
  // }
  // //slopeDen is now the "run" that results in an integer "rise", and therefore the minimal increment between ray hits
  // vec3 totalColor = vec3(0, 0, 0);
  // //TODO slow the slope increment as stars get distant enough to otherwise be missed ??
  // for(uint depth = 0;depth < 1024;depth++) {
  //   uvec3 l = ivec3(forwardDelta * depth * slopeDen + vec3(0.5, 0.5, 0.5)) + target.gridOrigin.xyz;
  //   //TODO initial value to compensate for in-sector offset of ray origin. For now, just assume we're in the center of the origin sector.
  //   uint value = 0;
  //   uvec3 p = uvec3(15131, 15583, 16067);
  //   for(uint blki = 0;blki < 32;blki++) {
  //     uvec3 lm = (l & 1) * p;
  //     value += lm.x + lm.y + lm.z;
  //     l >>= 1;
  //     p *= uvec3(547, 419, 283);
  //   }
  //   value &= modulusMask;
  //   if(value < 3)
  //     totalColor += colors[value] * (65536.0f / (depth*depth));
  // }
  // if(max(max(totalColor.x, totalColor.y), totalColor.z) > 1) totalColor = normalize(totalColor);
  // imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(totalColor, 1));

  // //6
  // vec3 forwardDelta = abs(norm);
  // forwardDelta = norm / max(max(forwardDelta.x, forwardDelta.y), forwardDelta.z);
  // vec3 totalColor = vec3(0, 0, 0);
  // uint blockSize = 2, blockExp = 1, depth = 64;
  // uint[passValues] targetValues;
  // for(uint i = 0;i < passValues;i++) targetValues[i] = i;
  // // if(target.size.w * depth > blockSize) {
  // //   blockSize <<= 1;
  // //   blockExp++;
  // // }
  // uvec3 l = ivec3(forwardDelta * depth + vec3(0.5, 0.5, 0.5)) + target.gridOrigin.xyz;
  // l >>= blockExp;
  // //44 with, 67 without: && max(max(l.x, l.y), l.z) > 0
  // for(uint blki = blockExp;blki < 32 && max(max(l.x, l.y), l.z) > 0;blki++) {
  //   uvec3 lm = (l & 1) * p[blki];
  //   uint offset = lm.x + lm.y + lm.z;
  //   for(uint i = 0;i < passValues;i++)
  //     targetValues[i] = targetValues[i] - offset;
  //   l >>= 1;
  // }
  // for(uint i = 0;i < passValues;i++)
  //   totalColor += colors[i] * (blockExp == 0 ? (targetValues[i] & modulusMask) == 0 ? 1 : 0 : scatterData.data[(blockExp * modulus + (targetValues[i] & modulusMask))/4][targetValues[i] & 3]);// * (4096.0f / (depth > 0 ? depth*depth : 1));
  // if(max(max(totalColor.x, totalColor.y), totalColor.z) > 1) totalColor = normalize(totalColor);//TODO replace with post-process bloom
  // imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(totalColor, 1));

  // //29
  // vec3 forwardDelta = abs(norm);
  // forwardDelta = norm / max(max(forwardDelta.x, forwardDelta.y), forwardDelta.z);
  // vec3 totalColor = vec3(0, 0, 0);
  // uint blockSize = 1, blockExp = 0;
  // //TODO miss block if ray center does not intersect inner star volume, for first N depth only (also accounting for float origin)
  // for(uint depth = 0;depth < 64;depth++) {//TODO higher max depth (256?) once proven
  //   uint[passValues] targetValues;
  //   for(uint i = 0;i < passValues;i++) targetValues[i] = i;
  //   // if(target.size.w * depth > blockSize) {
  //   //   blockSize <<= 1;
  //   //   blockExp++;
  //   // }
  //   uvec3 l = ivec3(forwardDelta * depth + vec3(0.5, 0.5, 0.5)) + target.gridOrigin.xyz;
  //   l >>= blockExp;
  //   //44 with, 67 without: && max(max(l.x, l.y), l.z) > 0
  //   for(uint blki = blockExp;blki < 32 && max(max(l.x, l.y), l.z) > 0;blki++) {
  //     uvec3 lm = (l & 1) * p[blki];
  //     uint offset = lm.x + lm.y + lm.z;
  //     for(uint i = 0;i < passValues;i++)
  // 	targetValues[i] = targetValues[i] - offset;
  //     l >>= 1;
  //   }
  //   for(uint i = 0;i < passValues;i++)
  //     totalColor += colors[i] * (blockExp == 0 ? (targetValues[i] & modulusMask) == 0 ? 1 : 0 : scatterData.data[(blockExp * modulus + (targetValues[i] & modulusMask))/4][targetValues[i] & 3]) * (4096.0f / (depth > 0 ? depth*depth : 1));
  // }
  // if(max(max(totalColor.x, totalColor.y), totalColor.z) > 1) totalColor = normalize(totalColor);//TODO replace with post-process bloom
  // imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(totalColor, 1));

  // //37
  // vec3 forwardDelta = abs(norm);
  // forwardDelta = norm / max(max(forwardDelta.x, forwardDelta.y), forwardDelta.z);
  // vec3 totalColor = vec3(0, 0, 0);
  // for(uint depth = 0;depth < 1024;depth++) {
  //   uvec3 l = ivec3(forwardDelta * depth + vec3(0.5, 0.5, 0.5)) + target.gridOrigin.xyz;
  //   uint value = 0;
  //   uvec3 p = uvec3(15131, 15583, 16067);
  //   for(uint blki = 0;blki < 32;blki++) {
  //     uvec3 lm = (l & 1) * p;
  //     value += lm.x + lm.y + lm.z;
  //     l >>= 1;
  //     p *= uvec3(547, 419, 283);
  //   }
  //   value &= modulusMask;
  //   if(value < 3)
  //     totalColor += colors[value] * (65536.0f / (depth*depth));
  // }
  // if(max(max(totalColor.x, totalColor.y), totalColor.z) > 1) totalColor = normalize(totalColor);
  // imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(totalColor, 1));


  // const uint blockSizeBits = 1, blockSizeMask = (1 << blockSizeBits) - 1;
  // const uint modulusMask = 0xFFFF;
  // const float modulusInv = pow(2, -16);
  // const uint modulusBits = 16, modulusMask = (1 << modulusBits) - 1;
  // const float modulusInv = pow(2, -modulusBits);
  // uvec3 l = gl_WorkGroupID.xyz + uvec3(1000000, 1000000, 1000000);//TODO remove fixed offset
  // uvec3 p = uvec3(15131, 15583, 16067);
  // uvec3 pinc = uvec3(547, 419, 283);
  // uint value = 0;
  // uvec3 lm;
  // for(uint blki = 0;blki <= 11;blki++) {
  //   lm = l & blockSizeMask;
  //   l >>= blockSizeBits;
  //   // lm *= lm*lm;
  //   value = value + (lm.x*p.x + lm.y*p.y + lm.z*p.z);
  //   p *= pinc;
  // }
  // // float tint = (value & modulusMask) < 30 ? 1 : 0;
  // float tint = float(value & modulusMask) / modulusMask;
  // // tint = tint >= 0.995f ? (tint - 0.99f) * 100 : 0;
  // imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(tint.xxx, 1));

// #define read fd[fdid^1]
// #define write fd[fdid]
// #define swap fdid ^= 1; for(uint fdidx = 0;fdidx < modulus;fdidx++) write[fdidx] = 0;
  // const uint maxBlockDepth = 4, blockSizeBits = 2, blockSizeMask = (1 << blockSizeBits) - 1, blockSize = 1 << blockSizeBits;
  // const uint modulusBits = 4, modulusMask = (1 << modulusBits) - 1, modulus = 1<<modulusBits;
  // const uvec3 p = uvec3(4000037, 5000627, 6000581);
  // uint[2][modulus] fd;
  // uint fdid = 0, i, j, b;
  // for(i = 0;i < blockSize;i++)
  //   fd[0][i] = fd[1][i] = 0;
  // for(i = 0;i < blockSize;i++)
  //   write[(i*i*i)&modulusMask] += 1;
  // swap;
  // for(i = 0;i < modulus;i++)
  //   write[(i*p.x)&modulusMask] = read[i];
  // swap;
  // for(i = 0;i < blockSize;i++)
  //   for(j = 0;j < modulus;j++)
  //     write[(j + i*p.y)&modulusMask] += read[j];
  // swap;
  // for(i = 0;i < blockSize;i++)
  //   for(j = 0;j < modulus;j++)
  //     write[(j + i*p.z)&modulusMask] += read[j];
  // for(b = 1;b <= maxBlockDepth;b++) {
  //   swap;
  //   for(i = 0;i < blockSize;i++)
  //     for(j = 0;j < modulus;j++)
  // 	write[(j + i*p.x)&modulusMask] += read[j];
  //   swap;
  //   for(i = 0;i < blockSize;i++)
  //     for(j = 0;j < modulus;j++)
  // 	write[(j + i*p.y)&modulusMask] += read[j];
  //   swap;
  //   for(i = 0;i < blockSize;i++)
  //     for(j = 0;j < modulus;j++)
  // 	write[(j + i*p.z)&modulusMask] += read[j];
  // }
  // swap;
  // dvec3 outColor = dvec3(read[0], read[1], read[2]);
  // outColor = normalize(outColor);
  // imageStore(color, ivec2(gl_WorkGroupID.xy), vec4(outColor, 1));
}
