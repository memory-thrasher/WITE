#include "stdafx.h"
#include "Object.h"

WITE::Transform Object::getTrans() {
  WITE::Transform ret;
  activeTransform.get(&ret);
  return ret;
}

