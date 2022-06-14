#include <iostream>
#include <cstring>

#include "../WITE/Database.hpp"

using namespace WITE::DB;

const struct entity_type dummy_et = { 1, NULL }; //type1, no update just push it around

class timebomb_t {
private:
  size_t frameCounter;
public:
  static void onUpdate(DBRecord* data, DBEntity* dbe) {
    timebomb_t dis;
    dbe->completeRead(&dis, data);
    if(dis.frameCounter++ > 200)
      dbe->destroy();
    dbe->write(&dis);
  }
  static constexpr struct entity_type et = { 2, &onUpdate };
};

struct entity_type types[] =
  {
   dummy_et,
   timebomb_t::et
  };

//#define assert(cond) if(!(cond)) std::cerr << "Error " << __FILE__ << ":" << __LINE__

const char* hw = "Hello World";
char out[200];

int main (int argc, char** argv) {
  Database* db = new Database(types, sizeof(types)/sizeof(types[0]), 100);
  DBEntity* e = db->allocate(dummy_et.typeId);
  e->write(hw, strlen(hw));
  e->read(out, strlen(hw));
  assert(strcmp(hw, out) == 0);
}

