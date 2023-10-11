//uniform data model

#include "CopyableArray.hpp"
#include "LiteralList.hpp"
#include "Vulkan.hpp"

namespace WITE {

  namespace Format {
    constexpr vk::Format R8int = vk::Format::eR8Sint;
    constexpr vk::Format R8uint = vk::Format::eR8Uint;
    constexpr vk::Format R16int = vk::Format::eR16Sint;
    constexpr vk::Format R16uint = vk::Format::eR16Uint;
    constexpr vk::Format R32int = vk::Format::eR32Sint;
    constexpr vk::Format R32uint = vk::Format::eR32Uint;
    constexpr vk::Format R32float = vk::Format::eR32Sfloat;
    constexpr vk::Format R64int = vk::Format::eR64Sint;
    constexpr vk::Format R64uint = vk::Format::eR64Uint;
    constexpr vk::Format R64float = vk::Format::eR64Sfloat;

    constexpr vk::Format RG8int = vk::Format::eR8G8Sint;
    constexpr vk::Format RG8uint = vk::Format::eR8G8Uint;
    constexpr vk::Format RG16int = vk::Format::eR16G16Sint;
    constexpr vk::Format RG16uint = vk::Format::eR16G16Uint;
    constexpr vk::Format RG32int = vk::Format::eR32G32Sint;
    constexpr vk::Format RG32uint = vk::Format::eR32G32Uint;
    constexpr vk::Format RG32float = vk::Format::eR32G32Sfloat;
    constexpr vk::Format RG64int = vk::Format::eR64G64Sint;
    constexpr vk::Format RG64uint = vk::Format::eR64G64Uint;
    constexpr vk::Format RG64float = vk::Format::eR64G64Sfloat;

    constexpr vk::Format RGB8int = vk::Format::eR8G8B8Sint;
    constexpr vk::Format RGB8uint = vk::Format::eR8G8B8Uint;
    constexpr vk::Format RGB16int = vk::Format::eR16G16B16Sint;
    constexpr vk::Format RGB16uint = vk::Format::eR16G16B16Uint;
    constexpr vk::Format RGB32int = vk::Format::eR32G32B32Sint;
    constexpr vk::Format RGB32uint = vk::Format::eR32G32B32Uint;
    constexpr vk::Format RGB32float = vk::Format::eR32G32B32Sfloat;
    constexpr vk::Format RGB64int = vk::Format::eR64G64B64Sint;
    constexpr vk::Format RGB64uint = vk::Format::eR64G64B64Uint;
    constexpr vk::Format RGB64float = vk::Format::eR64G64B64Sfloat;

    constexpr vk::Format RGBA8int = vk::Format::eR8G8B8A8Sint;
    constexpr vk::Format RGBA8uint = vk::Format::eR8G8B8A8Uint;
    constexpr vk::Format RGBA16int = vk::Format::eR16G16B16A16Sint;
    constexpr vk::Format RGBA16uint = vk::Format::eR16G16B16A16Uint;
    constexpr vk::Format RGBA32int = vk::Format::eR32G32B32A32Sint;
    constexpr vk::Format RGBA32uint = vk::Format::eR32G32B32A32Uint;
    constexpr vk::Format RGBA32float = vk::Format::eR32G32B32A32Sfloat;
    constexpr vk::Format RGBA64int = vk::Format::eR64G64B64A64Sint;
    constexpr vk::Format RGBA64uint = vk::Format::eR64G64B64A64Uint;
    constexpr vk::Format RGBA64float = vk::Format::eR64G64B64A64Sfloat;

    //do not use depth formats for vertex buffers
    constexpr vk::Format D16unorm = vk::Format::eD16Unorm;
    constexpr vk::Format D32sfloat = vk::Format::eD32Sfloat;
    constexpr vk::Format D16unormS8uint = vk::Format::eD16UnormS8Uint;
    constexpr vk::Format D24unormS8uint = vk::Format::eD24UnormS8Uint;
    constexpr vk::Format D32sfloatS8uint = vk::Format::eD32SfloatS8Uint;

  };

  typedef WITE::Collections::LiteralList<vk::Format> udm;//for vertexes and uniform buffers etc

#define defineUDM(NOM, ...) defineLiteralListScalar(vk::Format, NOM, __VA_ARGS__)

  namespace UDM {

    defineUDM(R8int, Format::R8int);
    defineUDM(R8uint, Format::R8uint);
    defineUDM(R16int, Format::R16int);
    defineUDM(R16uint, Format::R16uint);
    defineUDM(R32int, Format::R32int);
    defineUDM(R32uint, Format::R32uint);
    defineUDM(R32float, Format::R32float);
    defineUDM(R64int, Format::R64int);
    defineUDM(R64uint, Format::R64uint);
    defineUDM(R64float, Format::R64float);

    defineUDM(RG8int, Format::RG8int);
    defineUDM(RG8uint, Format::RG8uint);
    defineUDM(RG16int, Format::RG16int);
    defineUDM(RG16uint, Format::RG16uint);
    defineUDM(RG32int, Format::RG32int);
    defineUDM(RG32uint, Format::RG32uint);
    defineUDM(RG32float, Format::RG32float);
    defineUDM(RG64int, Format::RG64int);
    defineUDM(RG64uint, Format::RG64uint);
    defineUDM(RG64float, Format::RG64float);

    defineUDM(RGB8int, Format::RGB8int);
    defineUDM(RGB8uint, Format::RGB8uint);
    defineUDM(RGB16int, Format::RGB16int);
    defineUDM(RGB16uint, Format::RGB16uint);
    defineUDM(RGB32int, Format::RGB32int);
    defineUDM(RGB32uint, Format::RGB32uint);
    defineUDM(RGB32float, Format::RGB32float);
    defineUDM(RGB64int, Format::RGB64int);
    defineUDM(RGB64uint, Format::RGB64uint);
    defineUDM(RGB64float, Format::RGB64float);

    defineUDM(RGBA8int, Format::RGBA8int);
    defineUDM(RGBA8uint, Format::RGBA8uint);
    defineUDM(RGBA16int, Format::RGBA16int);
    defineUDM(RGBA16uint, Format::RGBA16uint);
    defineUDM(RGBA32int, Format::RGBA32int);
    defineUDM(RGBA32uint, Format::RGBA32uint);
    defineUDM(RGBA32float, Format::RGBA32float);
    defineUDM(RGBA64int, Format::RGBA64int);
    defineUDM(RGBA64uint, Format::RGBA64uint);
    defineUDM(RGBA64float, Format::RGBA64float);

    defineUDM(D16unorm, Format::D16unorm);
    defineUDM(D32sfloat, Format::D32sfloat);
    defineUDM(D16unormS8uint, Format::D16unormS8uint);
    defineUDM(D24unormS8uint, Format::D24unormS8uint);
    defineUDM(D32sfloatS8uint, Format::D32sfloatS8uint);

  }

  template<vk::Format> struct fieldTypeFor {}; //unsupported format for accessible data mapping

  template<> struct fieldTypeFor<vk::Format::eR8Sint> { typedef int8_t type; static constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR8Uint> { typedef uint8_t type; static constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR16Sint> { typedef int16_t type; static constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR16Uint> { typedef uint16_t type; static constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR32Sint> { typedef int32_t type; static constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR32Uint> { typedef uint32_t type; static constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR32Sfloat> { typedef float type; static constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR64Sint> { typedef int64_t type; static constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR64Uint> { typedef uint64_t type; static constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR64Sfloat> { typedef float type; static constexpr uint8_t qty = 1; };

  template<> struct fieldTypeFor<vk::Format::eR8G8Sint> { typedef int8_t type; static constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR8G8Uint> { typedef uint8_t type; static constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR16G16Sint> { typedef int16_t type; static constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR16G16Uint> { typedef uint16_t type; static constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR32G32Sint> { typedef int32_t type; static constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR32G32Uint> { typedef uint32_t type; static constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR32G32Sfloat> { typedef float type; static constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR64G64Sint> { typedef int64_t type; static constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR64G64Uint> { typedef uint64_t type; static constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR64G64Sfloat> { typedef float type; static constexpr uint8_t qty = 2; };

  template<> struct fieldTypeFor<vk::Format::eR8G8B8Sint> { typedef int8_t type; static constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR8G8B8Uint> { typedef uint8_t type; static constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR16G16B16Sint> { typedef int16_t type; static constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR16G16B16Uint> { typedef uint16_t type; static constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32Sint> { typedef int32_t type; static constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32Uint> { typedef uint32_t type; static constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32Sfloat> { typedef float type; static constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64Sint> { typedef int64_t type; static constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64Uint> { typedef uint64_t type; static constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64Sfloat> { typedef float type; static constexpr uint8_t qty = 3; };

  template<> struct fieldTypeFor<vk::Format::eR8G8B8A8Sint> { typedef int8_t type; static constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR8G8B8A8Uint> { typedef uint8_t type; static constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR16G16B16A16Sint> { typedef int16_t type; static constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR16G16B16A16Uint> { typedef uint16_t type; static constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32A32Sint> { typedef int32_t type; static constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32A32Uint> { typedef uint32_t type; static constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32A32Sfloat> { typedef float type; static constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64A64Sint> { typedef int64_t type; static constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64A64Uint> { typedef uint64_t type; static constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64A64Sfloat> { typedef float type; static constexpr uint8_t qty = 4; };

  template<vk::Format F> struct fieldFor {
    typedef fieldTypeFor<F>::type type[fieldTypeFor<F>::qty];
  };

  //tuple-like struct that matches the given description, which can also be mapped to a verted buffer
  template<udm U, size_t remaining = U.len> struct udmObject {
    fieldFor<U[U.len-remaining]>::type data;
    udmObject<U, remaining-1> next;
    //TODO more stuff
  };

  template<udm U> struct udmObject<U, 0> {};

}
