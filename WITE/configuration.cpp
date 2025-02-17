/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <unordered_map>
#include <cstring>
#include <memory>

#include "DEBUG.hpp"
#include "configuration.hpp"
#include "stdExtensions.hpp"

namespace WITE::configuration {

  struct configKeyHash {
    uint64_t operator()(const char* const& str) const {
      uint64_t key = 0;
      for(size_t i = 0;str[i] != '=' && str[i] != 0;i++)
	key += static_cast<uint32_t>(str[i]) << ((i % 8) * 8);
      return key;
    };
  };

  struct configKeyCompare {
    bool operator()(const char* const& l, const char* const& r) const {
      if(l == NULL && r == NULL) [[unlikely]] return true;
      if(l == NULL || r == NULL) [[unlikely]] return false;
      size_t i = 0;
      while(true) {
	if((l[i] == 0 || l[i] == '=') && (r[i] == 0 || r[i] == '=')) [[unlikely]] return true;
	if(l[i] != r[i]) [[unlikely]] return false;
	++i;
      };
    };
  };

  std::unordered_map<const char*, std::pair<const char*, const char*>, configKeyHash, configKeyCompare> options;

  //does not copy input strings, assumes this came from main and that the char*s will be valid forever
  void setOptions(int argc, const char** argv) {//static
    for(int i = 1;i < argc;i++) {
      auto& ref = options[argv[i]];
      if(!ref.first) {
	const char* eq = std::strchr(argv[i], '=');
	ref = { argv[i], eq ? eq+1 : NULL };
	LOG("CLI option: ", argv[i]);
      }
    }
  };

  const char* getOption(const char* key) {//static
    auto iter = options.find(key);
    return iter == options.end() ? NULL : iter->second.second;
  };

  bool contains(const char* opt) {
    return options.contains(opt);
  };

  //does copy the string data to new storage
  void appendOption(const char* opt) {
    if(!options.contains(opt)) {
      size_t len = std::strlen(opt) + 1;
      char* temp = new char[len];
      WITE::strcpy(temp, opt, len);
      char* value = std::strchr(temp, '=');
      options[temp] = { temp, value ? value+1 : NULL };
      LOG("appended option: ", temp);
    }
  };

  void dumpOptions(std::ostream& out) {
    for(auto& o : options)
      out << o.first << "\n";
  };

  bool getOptionBool(const char* key) {
    const char* v = getOption(key);
    if(v == NULL) return false;
    static constexpr const char* yesValues[] = { "yes", "y", "Y", "t", "T", "1" };
    for(const char* c : yesValues)
      if(strcmp(c, v) == 0)
	return true;
    return false;
  };

  int getOption(const char* key, int def) {
    const char* option = getOption(key);
    if(option) return std::stoi(option);
    else return def;
  };

  long getOption(const char* key, long def) {
    const char* option = getOption(key);
    if(option) return std::stol(option);
    else return def;
  };

  long long getOption(const char* key, long long def) {
    const char* option = getOption(key);
    if(option) return std::stoll(option);
    else return def;
  };

  unsigned int getOption(const char* key, unsigned int def) {
    const char* option = getOption(key);
    if(option) return (unsigned int)std::stoul(option);//no stoui
    else return def;
  };

  unsigned long getOption(const char* key, unsigned long def) {
    const char* option = getOption(key);
    if(option) return std::stoul(option);
    else return def;
  };

  unsigned long long getOption(const char* key, unsigned long long def) {
    const char* option = getOption(key);
    if(option) return std::stoull(option);
    else return def;
  };

  float getOption(const char* key, float def) {
    const char* option = getOption(key);
    if(option) return std::stod(option);
    else return def;
  };

  double getOption(const char* key, double def) {
    const char* option = getOption(key);
    if(option) return std::stof(option);
    else return def;
  };

};
