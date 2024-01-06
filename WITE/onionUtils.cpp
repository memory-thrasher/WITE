#include "onionUtils.hpp"

namespace WITE {

  syncLock onionStaticData::allDataMutex;
  std::map<hash_t, onionStaticData> onionStaticData::allOnionData;//static

};
