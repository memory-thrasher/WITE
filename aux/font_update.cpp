/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <iostream>
#include <string>
#include <vector>

struct point { int x, y; };

int indexOf(char c, char* str) {
  int i = 0;
  while(str[i] != 0) {
    if(str[i] == c)
      return i;
    i++;
  }
  return -1;
};

int indexOf(char c, char* str, int start) {
  int i = indexOf(c, str + start);
  return i == -1 ? i : i + start;
};

int main(int argc, char** argv) {
  constexpr float ox = 0, oy = 0, pitchx = 0.0714f, pitchy = -0.0714f;
  constexpr int maxlinelength = 1024, charCount = 128;
  char line[maxlinelength];
  char charname = 0;
  std::vector<point> verts;
  verts.emplace_back();//vert indecies in obj file are 1-based so just stick a dummy in idx 0
  std::vector<point> triangles[charCount];
  while(!std::cin.eof()) {
    std::cin.getline(line, maxlinelength);
    if(line[0] == 0 || line[1] != ' ') continue;//only care about v o and f, skip vt vn etc
    switch(line[0]) {
    case 'o':
      charname = line[2];
      break;
    case 'v':
      verts.push_back({
	  int((std::stof(line+2) - ox + pitchx/2) / pitchx),
	  int((std::stof(line+indexOf(' ', line, 3)) - oy + pitchy/2) / pitchy)
	});
      break;
    case 'f':
      int pos = 2;
      auto& t = triangles[int(charname)];
      t.push_back(verts[std::stoi(line+pos)]);
      pos = indexOf(' ', line, pos) + 1;
      t.push_back(verts[std::stoi(line+pos)]);
      pos = indexOf(' ', line, pos) + 1;
      t.push_back(verts[std::stoi(line+pos)]);
      pos = indexOf(' ', line, pos);
      if(pos != -1) {
	std::cerr << "error: non-triangle polygon found, aborting. Character: " << charname << " face: " << t.size()/3;
	return 1;
      }
      break;
    }
  }
  int rolling = 0;
  std::cout << "typedef WITE::udmObject<WITE::UDM::RG32uint> font_character_extent;\nconstexpr font_character_extent font_character_extents[" << charCount << "] = {\n";
  for(int i = 0;i < charCount;i++) {
    std::cout << "  { " << rolling << ", " << triangles[i].size() << " },\n";
    rolling += triangles[i].size();
  }
  std::cout << "};\n\ntypedef WITE::udmObject<WITE::UDM::RG8uint> font_point;\nconstexpr font_point font_triangles[] = {";
  for(int i = 0;i < charCount;i++)
    for(const auto& pt : triangles[i])
      std::cout << "{" << pt.x << "," << pt.y << "},";
  std::cout << "};\n";
  return 0;
};
