#pragma once

#include "stdafx.h"
#include "constants.h"
#include "exportTypes.h"

namespace WITE {

  export FILE* file_open(const char* path, const char* mode);
  export uint64_t timeNs();
  export void flush(FILE* file);
  export FILE* getERRLOGFILE();
  export void sleep(uint64_t us);
  export bool loadFile(const char* filename, size_t &len, const unsigned char** dataout);
  export bool endsWith(const char* str, const char* suffix);
  export bool startsWith(const char* str, const char* prefix);
  export uint64_t getFormat(size_t components, size_t componentSize, size_t componentType);
  export size_t getFileSize(const char* path);//0 for does not exist
  export bool strcmp_s(const std::string& a, const std::string& b);
  export void enterMainLoop(Database* db);//this call does not return. Set up type registration (with callbacks) first.

  template <typename _T, typename _Alloc = allocator<_T>> void export vectorPurge(std::vector<_T, _Alloc> v, _T o) {//Not thread safe. sync externally
    auto it = v->begin();
    while (it != v->end())
      if (o == *it)
	it = v->erase(it);
      else
	it++;
  }
  
}
