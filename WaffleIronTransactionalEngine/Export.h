#pragma once

#include "constants.h"
#include "exportTypes.h"
//#include "AtomicLinkedList.h"//TODO
#include "Database.h"

namespace WITE {

  export_dec void WITE_INIT(const char* gameName);
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
  export_dec void enterMainLoop();//this call does not return. Set up type registration (with callbacks) first.
#define database (*WITE::get_database())
  export_dec WITE::Database** get_database();
  export_dec void gracefulExit();//returns quickly, stops app gracefully after frame completion and db sync.

  template <typename _T, typename _Alloc = std::allocator<_T>>
  void export_def vectorPurge(std::vector<_T, _Alloc>* v, _T o) {//Not thread safe. sync externally
    auto it = v->begin();
    while (it != v->end())
      if (o == *it)
	it = v->erase(it);
      else
	it++;
  }
  
}
