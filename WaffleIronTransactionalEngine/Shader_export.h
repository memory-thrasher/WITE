#pragma once

namespace WITE {

  class export_def Shader {//not shared ptrs because these should be global in the game somewhere.
  public:
    struct resourceLayoutEntry {
      size_t type, stage, perInstance;
      void* moreData;
    };
    static Shader* make(const char* filepathWildcard, struct WITE::Shader::resourceLayoutEntry*, size_t resources);//static collection contains all
    static Shader* make(const char** filepath, size_t files, struct WITE::Shader::resourceLayoutEntry*, size_t resources);
    virtual ~Shader() = default;
  };

}