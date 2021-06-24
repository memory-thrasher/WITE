#pragma once

namespace WITE {

  class export_def Mesh {
  public:
    static std::shared_ptr<Mesh> make(MeshSource* source);
    Mesh(const Mesh&) = delete;
    virtual ~Mesh() = default;
  protected:
    Mesh() = default;
  };

};