/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

namespace WITE::configuration {

  void setOptions(int argc, char** argv);//ideally passed through from main BEFORE calling init
  char* getOption(const char* key);//options should be supplied as key=value
  bool contains(const char* opt);//for non-kvp options
  void appendOption(const char* opt);//not thread-safe with other read ops. Earliest match wins.
  void trimOptions();//remove duplicates and overridden kvps
  void dumpOptions(std::ostream& out);
  bool getOptionBool(const char* key);
  //generally speaking options will be optional. So allow the type of the default to provide the type of the output.
  int getOption(const char* key, int def);
  long getOption(const char* key, long def);
  long long getOption(const char* key, long long def);
  unsigned int getOption(const char* key, unsigned int def);
  unsigned long getOption(const char* key, unsigned long def);
  unsigned long long getOption(const char* key, unsigned long long def);
  float getOption(const char* key, float def);
  double getOption(const char* key, double def);

};
