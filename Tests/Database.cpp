#include <iostream>
#include <cstring>

//for timeout
#include <sys/time.h>
#include <sys/resource.h>

#include "../WITE/Database.hpp"

using namespace WITE::DB;

const struct entity_type dummy_et = { 1, NULL }; //type1, no update just push it around

class timebomb_t {
private:
  size_t frameCounter;
public:
  static void onUpdate(DBRecord* data, DBEntity* dbe) {
    Database* db = dbe->getDb();
    timebomb_t dis;
    dbe->completeRead(&dis, data);
    dis.frameCounter++;
    if(dis.frameCounter % 50 == 49)
      db->allocate(et);
    if(dis.frameCounter > 200)
      dbe->destroy();
    dbe->write(&dis);
    if(db->getFrame() > 500) db->shutdown();
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
  db->allocate(timebomb_t::et);
  //set up absolute timeout so if the db hangs, the test can eventually exit.
  //Linux kernel makes this real easy for it's own benefit. TODO this might need help on other platforms
  {
    struct rlimit rlim;
    getrlimit(RLIMIT_CPU, &rlim);
    rlim.rlim_max = WITE::Util::min(20, rlim.rlim_max);
    rlim.rlim_cur = rlim.rlim_max - 5;
  }
  db->start();
}

