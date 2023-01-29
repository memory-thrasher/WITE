#include "WorkBatch.hpp"
#include "SyncLock.hpp"
#include "Callback.hpp"

namespace WITE::Util {

  //S is intended to be an enum. Could be a primitive or PoD type.
  template<typename S> class StateSynchronizer {
  public:
    typedefCB(changeState, void, S, S, WorkBatch&);//old state, new state, worker
  private:
    SyncLock lock;
    S state;
    std::vector<WorkBatch> usingCurrentState;
    WorkBatch providingCurrentState;
    changeState change;
  public:
    PromisedStateTracker(S initial, changeState c) : state(initial), change(c) {};

    //does not change what type of promise p is. It must already be the proper type to use the new state.
    //the callback might spawn a new promise, which will be recorded and returned.
    //Callback must return a promise but may return is input.
    void inState(S s, WorkBatch& batch) {
      ScopeLock l(&lock);
      if(providingCurrentState == batch) {
	if(s == state)
	  return;
	ASSERT_TRAP(usingCurrentState.size() == 0, "transformable resouce deadlock");
	change(state, s, batch);
	return;
      }
      if(providingCurrentState)
	batch.mustHappenAfter(providingCurrentState);
      if(s == state) {
	usingCurrentState.push_back(batch);
      } else {
	change(state, s, batch);
	state = s;
	for(WorkBatch& consumer : usingCurrentState)
	  batch.mustHappenAfter(usingCurrentState);
	providingCurrentState = batch;
	usingCurrentState.clear();
      }
    };
  };

};
