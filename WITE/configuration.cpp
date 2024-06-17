/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <vector>
#include <string>
#include <cstring>
#include <memory>

#include "DEBUG.hpp"
#include "configuration.hpp"

namespace WITE::configuration {

  std::vector<std::unique_ptr<char[]>> options;

  void setOptions(int argc, char** argv) {//static
    for(int i = 0;i < argc;i++) {
      char* temp = new char[std::strlen(argv[i]) + 1];
      std::strcpy(temp, argv[i]);
      WARN("CLI option: ", temp);
      options.emplace_back(temp);//constructs unique_pointer around char array
    }
  };

  char* getOption(const char* key) {//static
    int keyLen = strlen(key);
    for(const auto& up : options) {
      char* kvp = up.get();
      if(strlen(kvp) > keyLen && kvp[keyLen] == '=' && strncmp(key, kvp, keyLen) == 0)
	return kvp+keyLen+1;//if no value given, this is a 0-length string (pointer to 0x00)
    }
    return NULL;
  };

  bool getOptionBool(const char* key) {
    char* v = getOption(key);
    if(v == NULL) return false;
    static constexpr const char* yesValues[] = { "yes", "y", "Y", "t", "T", "1" };
    for(const char* c : yesValues)
      if(strcmp(c, v) == 0)
	return true;
    return false;
  };

  int getOption(const char* key, int def) {
    char* option = getOption(key);
    if(option) return std::stoi(option);
    else return def;
  };

  long getOption(const char* key, long def) {
    char* option = getOption(key);
    if(option) return std::stol(option);
    else return def;
  };

  long long getOption(const char* key, long long def) {
    char* option = getOption(key);
    if(option) return std::stoll(option);
    else return def;
  };

  unsigned int getOption(const char* key, unsigned int def) {
    char* option = getOption(key);
    if(option) return (unsigned int)std::stoul(option);//no stoui
    else return def;
  };

  unsigned long getOption(const char* key, unsigned long def) {
    char* option = getOption(key);
    if(option) return std::stoul(option);
    else return def;
  };

  unsigned long long getOption(const char* key, unsigned long long def) {
    char* option = getOption(key);
    if(option) return std::stoull(option);
    else return def;
  };

};
