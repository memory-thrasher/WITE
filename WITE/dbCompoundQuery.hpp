#pragma once

#include "dbTable.hpp"

namespace WITE {

  //use a compound query if multiple objects must be read and/or written without any of them changing. Avoid when possible.
  //reads of a prior frame's data is preferred because it won't change and so does not need to be locked
  class dbCompoundQuery {
  private:
    std::set<syncLock*> mutexes;//row locks. Sorted by address so the locking order is consistent.
    std::vector<scopeLock> locks;
    //TODO implement if needed. Can't think of a good reason to do this, nor proof that it won't ever be needed.
  };
