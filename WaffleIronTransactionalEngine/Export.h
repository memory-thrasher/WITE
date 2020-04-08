#pragma once

#include "stdafx.h"
#include "constants.h"
#include "exportTypes.h"
#include "AtomicLinkedList.h"
#include "Database.h"

namespace WITE {

  export_dec FILE* file_open(const char* path, const char* mode);
  export_dec uint64_t timeNs();
  export_dec void flush(FILE* file);
  export_dec FILE* getERRLOGFILE();
  export_dec void sleep(uint64_t us);
  export_dec bool loadFile(const char* filename, size_t &len, const unsigned char** dataout);
  export_dec bool endsWith(const char* str, const char* suffix);
  export_dec bool startsWith(const char* str, const char* prefix);
  export_dec uint64_t getFormat(size_t components, size_t componentSize, size_t componentType);
  export_dec size_t getFileSize(const char* path);//0 for does not exist
  export_dec bool strcmp_s(const std::string& a, const std::string& b);
  export_dec void enterMainLoop(class Database* db);//this call does not return. Set up type registration (with callbacks) first.

  template <typename _T, typename _Alloc = allocator<_T>>
  void export_def vectorPurge(std::vector<_T, _Alloc>* v, _T o) {//Not thread safe. sync externally
    auto it = v->begin();
    while (it != v->end())
      if (o == *it)
	it = v->erase(it);
      else
	it++;
  }
  
}
