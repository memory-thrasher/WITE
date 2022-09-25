#include <iostream>
#include <cstring>
#include <time.h>
#include <signal.h>

//for timeout
#include <sys/time.h>
#include <sys/resource.h>

#include "../WITE/Database.hpp"

using namespace WITE::DB;

const struct entity_type dummy_et = entity_type(1); //type1, no update just push it around

std::atomic_uint64_t hits = 0, allocated, deallocated, spunUp, spunDown, skipped;

#define FRAME_COUNT 5000

class timebomb_t {
private:
  uint64_t frameCounter;
public:
  static void onUpdate(DBRecord* data, DBEntity* dbe) {
    hits++;
    Database* db = dbe->getDb();
    auto dbf = db->getFrame();
    timebomb_t dis;
    dbe->completeRead(&dis, data);
    dis.frameCounter++;
    //LOG("Updating: ", dbe->getId(), ", frame: ", dbf, ", new value: ", dis.frameCounter);
    if(dis.frameCounter % 50 == 7) {
      if(!db->allocate(et))
	skipped++;
	// std::cerr << "Failed to spawn, out of db space" << std::endl;
    }
    if(dis.frameCounter >= 150) {
      dbe->destroy(data);
    }
    dbe->write(&dis);
    if(dbf >= FRAME_COUNT) db->shutdown();
  };
  static void onAllocate(DBEntity* dbe) {
    //std::cout << std::dec << "Spawning " << dbe->getId() << " at frame " << dbe->getDb()->getFrame() << std::endl;
    allocated++;
    timebomb_t dis;
    dis.frameCounter = 0;
    dbe->write(&dis);
  };
  static void onDeallocate(DBEntity* dbe) {
    //std::cout << std::dec << "Destroying " << dbe->getId() << " at frame " << dbe->getDb()->getFrame() << std::endl;
    deallocated++;
  };
  static void onSpinUp(DBEntity* dbe) {
    spunUp++;
  };
  static void onSpinDown(DBEntity* dbe) {
    spunDown++;
  };
  static constexpr struct entity_type et = entity_type::makeFrom<timebomb_t>(2);
};

struct entity_type types[] =
  {
   dummy_et,
   timebomb_t::et
  };

const char* hw = "Hello World";
char out[200];

int main (int argc, char** argv) {
  assert(types[1].onUpdate);
  assert(WITE::Util::min(20, 200) == 20);
  //set up absolute timeout so if the db hangs, the test can eventually exit.
  //Linux kernel makes this real easy for it's own benefit. TODO this might need help on other platforms
  if(argc < 2) {
    struct rlimit rlim;
    getrlimit(RLIMIT_CPU, &rlim);
    rlim.rlim_max = WITE::Util::min<uint64_t, uint64_t>(200*DB_THREAD_COUNT, rlim.rlim_max);
    rlim.rlim_cur = rlim.rlim_max - 5;
    setrlimit(RLIMIT_CPU, &rlim);
    getrlimit(RLIMIT_CPU, &rlim);
    std::cout << "Limiting cpu time to " << rlim.rlim_cur << " seconds." << std::endl;
  }
  Database* db = new Database(types, sizeof(types)/sizeof(types[0]), 37601);
  DBEntity* e = db->allocate(dummy_et.typeId);
  assert(e);
  e->write(hw, strlen(hw));
  e->read(out, strlen(hw));
  assert(strcmp(hw, out) == 0);
  assert(db->allocate(timebomb_t::et));
  std::cout << "Starting db" << std::endl;
  //timer stuff
  static const struct itimerspec MAX_TIME { { 0, 0 }, { 100, 0 } };
  timer_t timer;
  struct sigevent noop;
  noop.sigev_notify = SIGEV_NONE;
  assert(!timer_create(CLOCK_REALTIME, &noop, &timer));
  assert(!timer_settime(timer, 0, &MAX_TIME, NULL));
  //do the thing
  db->start();
  //more timer stuff
  struct itimerspec time;
  assert(!timer_gettime(timer, &time));
  double timeR = 1.0 * (MAX_TIME.it_value.tv_sec - time.it_value.tv_sec) + (MAX_TIME.it_value.tv_nsec - time.it_value.tv_nsec) / 1000000000.0;
  delete db;
  std::cout << std::dec << "Time: " << timeR << " (" << (FRAME_COUNT/timeR) << " FPS)" << std::endl;
  std::cout << std::dec << "Updates: " << hits << std::endl;
  std::cout << std::dec << "Allocated: " << allocated << std::endl;
  std::cout << std::dec << "Deallocated: " << deallocated << std::endl;
  std::cout << std::dec << "Spun up: " << spunUp << std::endl;
  std::cout << std::dec << "Spun down: " << spunDown << std::endl;
  // assert(hits == 5760620);
  // assert(37957 == deallocated);//do not deallocate when closing the db, it can be resumed from file (or else it'll be discarded)
  // assert(40076 == allocated);
  // assert(40076 == spunUp);
  // assert(40076 == spunDown);
  assert(spunDown == spunUp);
}

