#pragma once

#include "dbTemplateStructs.hpp"
#include "dbUtils.hpp"
#include "dbtableTuple.hpp"

namespace WITE {

  /*each type given to the db must have the following static members or typedefs:
    uint64_t typeId
    std::string dbFileId
optional members:
    update(uint64_t objectId) //called every frame
    allocated(uint64_t objectId) //called when object is first created (should init persistent data)
    freed(uint64_t objectId) //called when object is destroyed (rare, should free db children)
    spunUp(uint64_t objectId) //called when object is created or loaded from disk (should set up any vulkan or other transients)
    spunDown(uint64_t objectId) //called when the object is destroyed or when the game is closing (should clean up transients)
   */

  template<class... TYPES> class database {
  private:
    std::atomic_uint64_t currentFrame = 0, flushingFrame = 0;
    dbtableTuple<TYPES...> bobby;//327
    threadPool threads;//dedicated thread pool so we can tell when all frame data is done
    std::atomic_bool_t backupInProgress;
    const std::string backupTarget;
    thread* backupThread;

    template<class T, class... REST> inline void backupTable() {
      //log files won't be backed up, and won't be applied while the backup is running, so just get all the mdfs to a common frame and let all new data flow sit around in the logs
#error TODO rework dbtable load to allow log file to not exist (purge any log links in the mdf)
    };

    void backupThreadEntry() {
      backupTable<TYPES...>();
      backupInProgress = false;
    };

  public:
    database(std::string& basedir, bool clobber) : bobby(basedir, clobber) {};

    //process a single frame, part 1: updates only
    void updateTick() {
      //
    };

    //process a single frame, part 2: logfile maintenance
    void maintenanceTick() {
      if(!backupInProgress.load(std::memory_order_relaxed)) { //don't flush logs when the mdfs are being copied
	//
      }
    };

    bool requestBackup(const std::string& outdir) {
      bool t = false;
      if(backupInProgress.compare_exchange_strong(t, true, std::memory_order_relaxed)) {
	backupTarget = outdir;
	if(backupThread) {
	  backupThread->join();//should be immediately joinable since it's done
	  delete backupThread;
	}
	//callback allocation is forgivable for file io
	backupThread = thread::spawnThread(thread::threadEntry_t_F::make(this, backupThreadEntry));
      }
    };

  };

};
