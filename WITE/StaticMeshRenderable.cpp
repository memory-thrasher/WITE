#include "StaticMeshRenderable.hpp"

namespace WITE::GPU {

  std::map<void*, staticMeshCache> staticMeshCache::cache;
  Util::SyncLock staticMeshCache::bufferMutex;

}
