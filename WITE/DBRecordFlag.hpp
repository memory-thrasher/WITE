#pragma once

#include <limits>

namespace WITE::DB {

  typedef uint16_t DBRecordFlag_t;

  enum class DBRecordFlag : DBRecordFlag_t
    {
     none = 0,
     allocated = (1 << 0),
     head_node = (1 << 1),
     has_next  = (1 << 2),
     all = has_next | head_node | allocated,
    };

  constexpr inline DBRecordFlag operator|(DBRecordFlag L, DBRecordFlag R) {
    return static_cast<DBRecordFlag>(static_cast<DBRecordFlag_t>(L) | static_cast<DBRecordFlag_t>(R));
  };

  constexpr inline DBRecordFlag operator&(DBRecordFlag L, DBRecordFlag R) {
    return static_cast<DBRecordFlag>(static_cast<DBRecordFlag_t>(L) & static_cast<DBRecordFlag_t>(R));
  };

  constexpr inline DBRecordFlag operator^(DBRecordFlag L, DBRecordFlag R) {
    return static_cast<DBRecordFlag>(static_cast<DBRecordFlag_t>(L) ^ static_cast<DBRecordFlag_t>(R));
  };

  constexpr inline DBRecordFlag operator~(DBRecordFlag L) {
    return static_cast<DBRecordFlag>(~static_cast<DBRecordFlag_t>(L));
  };

  inline DBRecordFlag operator|=(DBRecordFlag& L, DBRecordFlag R) {
    return L = L | R;
  };

  inline DBRecordFlag operator&=(DBRecordFlag& L, DBRecordFlag R) {
    return L = L & R;
  };

  inline DBRecordFlag operator^=(DBRecordFlag& L, DBRecordFlag R) {
    return L = L ^ R;
  };

  inline bool operator!=(DBRecordFlag L, int R) {
    return static_cast<int>(L) != R;
  };

  inline bool operator==(DBRecordFlag L, int R) {
    return static_cast<int>(L) == R;
  };

  inline bool operator>>(const DBRecordFlag L, const DBRecordFlag R) {
    return (L & R) == R;
  };

}
