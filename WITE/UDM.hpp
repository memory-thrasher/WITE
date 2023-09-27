//uniform data model

#include "OversizedCopyableArray.hpp"

namespace WITE {

  // enum class dataType : uint8_t {
  //   integer, floating, fixed;
  // };

  // struct field {
  //   bool isSigned;
  //   uint8_t bits;
  //   dataType type;
  //   uint32_t cnt;
  // };

  typedef LiteralList<field> udm;//for vertexes and uniform buffers etc

  #error TODO replace this with the vulkan constants VK_FORMAT_R32G32etc

  // template<bool isSigned, uint8_t bits, dataType type> struct fieldTypeFor {};

  // template<> struct fieldTypeFor<true, 8, dataType.integer> { typedef int8_t type; };
  // template<> struct fieldTypeFor<true, 16, dataType.integer> { typedef int16_t type; };
  // template<> struct fieldTypeFor<true, 32, dataType.integer> { typedef int32_t type; };
  // template<> struct fieldTypeFor<true, 64, dataType.integer> { typedef int64_t type; };
  // template<> struct fieldTypeFor<false, 8, dataType.integer> { typedef uint8_t type; };
  // template<> struct fieldTypeFor<false, 16, dataType.integer> { typedef uint16_t type; };
  // template<> struct fieldTypeFor<false, 32, dataType.integer> { typedef uint32_t type; };
  // template<> struct fieldTypeFor<false, 64, dataType.integer> { typedef uint64_t type; };
  // template<> struct fieldTypeFor<true, 32, dataType.floating> { typedef float type; };
  // template<> struct fieldTypeFor<true, 64, dataType.floating> { typedef double type; };

  template<field F> struct fieldFor {
    typedef fieldTypeFor<F.isSigned, F.bits, F.type>::type[F.cnt] type;
  };

  //tuple-like struct that matches the given description, which can also be mapped to a verted buffer
  template<udm U, size_t remaining = U.len> struct udmObject {
    fieldFor<U[U.len-remaining]>::type data;
    udmObject<U, remaining-1> next;
    //TODO more stuff
  };

  template<udm U> struct udmObject<U, 0> {};

}
