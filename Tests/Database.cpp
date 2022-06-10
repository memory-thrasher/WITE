#include <iostream>
#include <cstring>

#include "../WITE/Database.hpp"

using namespace WITE::DB;

struct entity_type dummy = { 1, NULL }; //type1, no update just push it around

struct entity_type types[] =
  {
   dummy
  };

//#define assert(cond) if(!(cond)) std::cerr << "Error " << __FILE__ << ":" << __LINE__

int main (int argc, char** argv) {
  Database* db = new Database(types, sizeof(types)/sizeof(types[0]), 100);
  DBEntity* e = db->allocate(dummy.typeId);
  char hw[] = "Hello World";
  e->write(hw, sizeof(hw));
  char out[200];
  e->read(&out, sizeof(hw));
  assert(strcmp(hw, out) == 0);
}

