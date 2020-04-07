#include "stdafx.h"
#include "Object.h"

Transform Object::getTrans() {
  Transform ret;
  activeTransform.get(&ret);
  return ret;
}

