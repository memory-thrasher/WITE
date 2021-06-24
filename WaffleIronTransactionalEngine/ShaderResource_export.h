#pragma once

namespace WITE {

  class export_def ShaderResource {
  public:
    virtual void load(rawDataSource) = 0;
    virtual void* map() = 0;
    virtual void unmap() = 0;
    virtual size_t getSize() = 0;
    //virtual BackedBuffer* getBuffer() = 0;
    virtual ~ShaderResource() = default;
  };

};
