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

void Object::setNameF(const char* fmt, ...) {
  char buffer[4096];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  setName(buffer);
}

void Object::setName(const char* n) {
  size_t l = strlen(n) + 1;
  name = static_cast<char*>(malloc(l));
  memcpy(name, n, l);
}

const char* Object::getName() {
  return name;
}
