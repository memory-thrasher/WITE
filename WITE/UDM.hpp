//uniform data model

#include "CopyableArray.hpp"
#include "LiteralList.hpp"
#include "Vulkan.hpp"

namespace WITE::Export {

  typedef LiteralList<vk::Format> udm;//for vertexes and uniform buffers etc

  enum class Format : vk::Format {
    eR8int = Vk::Format::eR8_SINT,
    eR8uint = vk::Format::eR8_UINT,
    eR16int = vk::Format::eR16_SINT,
    eR16uint = vk::Format::eR16_UINT,
    eR32int = vk::Format::eR32_SINT,
    eR32uint = vk::Format::eR32_UINT,
    eR32float = vk::Format::eR32_SFLOAT,
    eR64int = vk::Format::eR64_SINT,
    eR64uint = vk::Format::eR64_UINT,
    eR64float = vk::Format::eR64_SFLOAT,

    eRG8int = vk::Format::eR8G8_SINT,
    eRG8uint = vk::Format::eR8G8_UINT,
    eRG16int = vk::Format::eR16G16_SINT,
    eRG16uint = vk::Format::eR16G16_UINT,
    eRG32int = vk::Format::eR32G32_SINT,
    eRG32uint = vk::Format::eR32G32_UINT,
    eRG32float = vk::Format::eR32G32_SFLOAT,
    eRG64int = vk::Format::eR64G64_SINT,
    eRG64uint = vk::Format::eR64G64_UINT,
    eRG64float = vk::Format::eR64G64_SFLOAT,

    eRGB8int = vk::Format::eR8G8B8_SINT,
    eRGB8uint = vk::Format::eR8G8B8_UINT,
    eRGB16int = vk::Format::eR16G16B16_SINT,
    eRGB16uint = vk::Format::eR16G16B16_UINT,
    eRGB32int = vk::Format::eR32G32B32_SINT,
    eRGB32uint = vk::Format::eR32G32B32_UINT,
    eRGB32float = vk::Format::eR32G32B32_SFLOAT,
    eRGB64int = vk::Format::eR64G64B64_SINT,
    eRGB64uint = vk::Format::eR64G64B64_UINT,
    eRGB64float = vk::Format::eR64G64B64_SFLOAT,

    eRGBA8int = vk::Format::eR8G8B8A8_SINT,
    eRGBA8uint = vk::Format::eR8G8B8A8_UINT,
    eRGBA16int = vk::Format::eR16G16B16A16_SINT,
    eRGBA16uint = vk::Format::eR16G16B16A16_UINT,
    eRGBA32int = vk::Format::eR32G32B32A32_SINT,
    eRGBA32uint = vk::Format::eR32G32B32A32_UINT,
    eRGBA32float = vk::Format::eR32G32B32A32_SFLOAT,
    eRGBA64int = vk::Format::eR64G64B64A64_SINT,
    eRGBA64uint = vk::Format::eR64G64B64A64_UINT,
    eRGBA64float = vk::Format::eR64G64B64A64_SFLOAT
  }

#define defineUDM(NOM, ...) defineLiteralListScalar(vk::Format, NOM, __VA_ARGS__)

  defineUDM(

  template<vk::Format> struct fieldTypeFor {}; //unsupported format for accessible data mapping

  template<> struct fieldTypeFor<vk::Format::eR8_SINT> { typedef int8_t type; constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR8_UINT> { typedef uint8_t type; constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR16_SINT> { typedef int16_t type; constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR16_UINT> { typedef uint16_t type; constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR32_SINT> { typedef int32_t type; constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR32_UINT> { typedef uint32_t type; constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR32_SFLOAT> { typedef float type; constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR64_SINT> { typedef int64_t type; constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR64_UINT> { typedef uint64_t type; constexpr uint8_t qty = 1; };
  template<> struct fieldTypeFor<vk::Format::eR64_SFLOAT> { typedef float type; constexpr uint8_t qty = 1; };

  template<> struct fieldTypeFor<vk::Format::eR8G8_SINT> { typedef int8_t type; constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR8G8_UINT> { typedef uint8_t type; constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR16G16_SINT> { typedef int16_t type; constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR16G16_UINT> { typedef uint16_t type; constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR32G32_SINT> { typedef int32_t type; constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR32G32_UINT> { typedef uint32_t type; constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR32G32_SFLOAT> { typedef float type; constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR64G64_SINT> { typedef int64_t type; constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR64G64_UINT> { typedef uint64_t type; constexpr uint8_t qty = 2; };
  template<> struct fieldTypeFor<vk::Format::eR64G64_SFLOAT> { typedef float type; constexpr uint8_t qty = 2; };

  template<> struct fieldTypeFor<vk::Format::eR8G8B8_SINT> { typedef int8_t type; constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR8G8B8_UINT> { typedef uint8_t type; constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR16G16B16_SINT> { typedef int16_t type; constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR16G16B16_UINT> { typedef uint16_t type; constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32_SINT> { typedef int32_t type; constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32_UINT> { typedef uint32_t type; constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32_SFLOAT> { typedef float type; constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64_SINT> { typedef int64_t type; constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64_UINT> { typedef uint64_t type; constexpr uint8_t qty = 3; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64_SFLOAT> { typedef float type; constexpr uint8_t qty = 3; };

  template<> struct fieldTypeFor<vk::Format::eR8G8B8A8_SINT> { typedef int8_t type; constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR8G8B8A8_UINT> { typedef uint8_t type; constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR16G16B16A16_SINT> { typedef int16_t type; constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR16G16B16A16_UINT> { typedef uint16_t type; constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32A32_SINT> { typedef int32_t type; constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32A32_UINT> { typedef uint32_t type; constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR32G32B32A32_SFLOAT> { typedef float type; constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64A64_SINT> { typedef int64_t type; constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64A64_UINT> { typedef uint64_t type; constexpr uint8_t qty = 4; };
  template<> struct fieldTypeFor<vk::Format::eR64G64B64A64_SFLOAT> { typedef float type; constexpr uint8_t qty = 4; };

  template<vk::Format F> struct fieldFor {
    typedef fieldTypeFor<F>::type[fieldTypeFor<F>::qty] type;
  };

  //tuple-like struct that matches the given description, which can also be mapped to a verted buffer
  template<udm U, size_t remaining = U.len> struct udmObject {
    fieldFor<U[U.len-remaining]>::type data;
    udmObject<U, remaining-1> next;
    //TODO more stuff
  };

  template<udm U> struct udmObject<U, 0> {};

}
