#include "stdafx.h"
#include "Object.h"

Transform getTrans() {
  Transform ret;
  activeTransform.get(&ret);
  return ret;
}

