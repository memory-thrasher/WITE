#include "stdafx.h"
#include "Object.h"

WITE::Transform Object::getTrans() {
  WITE::Transform ret;
  activeTransform.get(&ret);
  return ret;
}

void Object::pushTrans(WITE::Transform* t) {
  activeTransform.put(t);
}

WITE::Object* WITE::Object::make(WITE::Database::Entry start, size_t transformOffset, WITE::Database::Entry* map) {
  return new ::Object(start, transformOffset, map);
}
