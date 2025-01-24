/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <iostream>

class A {
public:
  int* data;
  A(int i) : data(new int()) {
    *data = i;
    std::cout << "Set to " << i << std::endl;
  };
  virtual ~A() {
    std::cout << "Destructor fired: " << (*data) << std::endl;
    delete data;
  };
};

class B : public A {
public:
  B() : A(6) {};
  virtual ~B() {
    std::cout << "Destructor B fired" << std::endl;
  };
};

int main(int argc, char** argv) {
  A* a = new B();
  delete a;
};

