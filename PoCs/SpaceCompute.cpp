/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <memory>
#include <iostream>
#include <string.h>

constexpr unsigned long modulusBits = 16, modulusMask = ~((~0ul) << modulusBits), modulus = 1 << modulusBits,
	    pi[3] = { 547, 419, 283 }, hitTypes = 3, maxDepth = 8, maxBlockSize = 1<<(maxDepth-1);
unsigned long fds[maxDepth][modulus], tempFd[modulus];//>8mb, so not on thread stack
unsigned int p[maxDepth][3];

int main(int argc, const char** argv) {
  p[0][0] = 15131;
  p[0][1] = 15583;
  p[0][2] = 16067;
  for(unsigned int i = 1;i < maxDepth;i++)
    for(int axis = 0;axis < 3;axis++)
      p[i][axis] = p[i-1][axis] * pi[axis];
  unsigned long b;
  memset(fds, 0, sizeof(fds));
  fds[0][0] = 1;
  for(b = 1;b < maxDepth;b++) {
    memcpy(fds[b], fds[b-1], sizeof(tempFd));
    for(int axis = 0;axis < 3;axis++) {
      memcpy(tempFd, fds[b], sizeof(tempFd));
      for(int i = 0;i < modulus;i++) {
	fds[b][(i + p[b-1][axis]) & 0xFFFF] += tempFd[i];
	// if(fds[b][(i + p[b-a][axis]) & 0xFFFF] > modulusMask) {
	//   std::cout << "32-bit wraparound at depth: " << b;
	//   return 0;
	// }
      }
    }
  }
  // for(int j = 0;j < modulus;j++)
  //   std::cout << fds[maxDepth-1][j] << ": " << j << '\n';
  //output the complete c/glsl initializer list to be included into applications/shaders
  std::cout << "const unsigned int scatplot_freq_dist[" << maxDepth << " * " << modulus << "] = {\n  ";
  for(unsigned int i = 0;i < maxDepth;i++) {
    for(unsigned int j = 0;j < modulus;j++) {
      if((i + j) % 40 == 39)
	std::cout << "\n  ";
      if(i || j)
	std::cout << ", ";
      std::cout << fds[i][j];
    }
  }
  std::cout << " };\n//scatplot_freq_dist\n";
  // std::cout << "\nconst uvec3[" << maxDepth << "] p = { ";
  // for(unsigned int i = 0;i < maxDepth;i++) {
  //   if(i) std::cout << ", ";
  //   std::cout << "uvec3(";
  //   for(int j = 0;j < 3;j++) {
  //     if(j) std::cout << ", ";
  //     std::cout << (p[i][j] & modulusMask);
  //   }
  //   std::cout << ")";
  // }
  // std::cout << " };\n";
  //now do it the slow way to check they're the same
  memset(tempFd, 0, sizeof(tempFd));
  for(int x = 0;x < maxBlockSize;x++)
    for(int y = 0;y < maxBlockSize;y++)
      for(int z = 0;z < maxBlockSize;z++) {
	int value = 0;
	int lx = x, ly = y, lz = z;
	for(b = 0;b < 32;b++) {
	  if(lx & 1) value += p[b][0];
	  if(ly & 1) value += p[b][1];
	  if(lz & 1) value += p[b][2];
	  lx >>= 1;
	  ly >>= 1;
	  lz >>= 1;
	}
	tempFd[value & modulusMask]++;
      }
  for(int i = 0;i < modulus;i++)
    if(tempFd[i] != fds[maxDepth-1][i])
      std::cout << "FD mismatch! " << i << ": fast=" << fds[maxDepth-1][i] << " slow=" << tempFd[i] << "\n";
};

