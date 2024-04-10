#pragma once

#include "dbtable.hpp"

namespace WITE {

  //use a compound query if multiple objects must be read and/or written without any of them changing. Avoid when possible.
  //reads of a prior frame's data is preferred because it won't change and so does not need to be locked
  class dbCompoundQuery {
  private:
    std::set<syncLock*> mutexes;
    std::vector<scopeLock> locks;
    //TODO finish this when the db api is ready
  };
