#pragma once

#include "dbTemplateStructs.hpp"
#include "dbUtils.hpp"
#include "dbtableTuple.hpp"

namespace WITE {

  /*each type given to the db must have the following static members or typedefs:
    uint64_t typeId
    std::string dbFileId
optional members:
    update(uint64_t objectId) //called every frame
    allocated(uint64_t objectId) //called when object is first created (should init persistent data)
    freed(uint64_t objectId) //called when object is destroyed (rare, should free db children)
    spunUp(uint64_t objectId) //called when object is created or loaded from disk (should set up any vulkan or other transients)
    spunDown(uint64_t objectId) //called when the object is destroyed or when the game is closing (should clean up transients)
   */

  template<class... TYPES> struct database {

    std::atomic_uint64_t currentFrame = 1, flushingFrame = 0;
    dbtableTuple<TYPES...> bobby;//327

    //

  };

};
