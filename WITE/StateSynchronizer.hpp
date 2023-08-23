#include "WorkBatch.hpp"
#include "SyncLock.hpp"
#include "Callback.hpp"

namespace WITE::GPU {

  class StateSynchronizerBase {
  public:
    constexpr static size_t timelineWidth = 64, timelineDepth = 8;
    struct waitCollection {
      SemaphorePointInTime[timelineWidth] waitSems;
      size_t cnt;
    };

  private:

    struct timelineEntry {
      SemaphorePointInTime semaphore;
    };

    struct timelinePoint {
      std::atomic_uint64_t slotOccupiedMap, countWaitingOnSpace;
      timelineEntry[timelineWidth] pre;
      SemaphorePointInTime post;
    };

    timelinePoint[timelineDepth] timeline;
    size_t currentTime, timelineDepthUsed;

  protected:
    Util::SyncLock lock;
    StateSynchronizerBase() = default;
    virtual ~StateSynchronizerBase() = default;

  public:
    void recordFutureLock();
    void acquireRecordedLock(waitCollection* ret, SemaphorePointInTime releaser);
    void acquireLock(waitCollection* ret, SemaphorePointInTime releaser);
  };

  template<typename S> requires requires(const S& sa, const S& sb) { sa == sb; }
  class StateSynchronizer : public StateSynchronizerBase {
  public:
    typedefCB(changeState_t, void, S*, S*, WorkBatch&);//old state, (writable) new state, worker.
  private:
    S state;
    changeState_t changeState;

  public:
    StateSynchronizer(S initial, changeState c) : state(initial), change(c) {};
    StateSynchronizer() {};//allocation dummy
    virtual ~StateSynchronizer() override = default;

    void inState(S& s, WorkBatch cmd) {
      //
    };

  };

//   //S is intended to be an enum. Could be a primitive or PoD type.
//   template<typename S> requires requires(const S& sa, const S& sb) { sa == sb; }
//   class StateSynchronizer {
//   public:
//     typedefCB(changeState, void, S, S*, WorkBatch&);//old state, (writable) new state, worker.
//   private:
//     Util::SyncLock lock;
//     S state;
//     std::vector<WorkBatch> usingCurrentState;
//     WorkBatch providingCurrentState;
//     changeState change;
//     bool firstUse = true;
//   public:
//     StateSynchronizer(S initial, changeState c) : state(initial), change(c) {};
//     StateSynchronizer() {};//allocation dummy

//     void addConsumer(WorkBatch& batch) {
//       batch.mustHappenAfter(providingCurrentState);
//     }

//     void inState(S s, WorkBatch& batch) {
//       Util::ScopeLock l(&lock);
//       if(providingCurrentState == batch) {
// 	if(s == state)
// 	  return;
// 	ASSERT_TRAP(usingCurrentState.size() == 0, "transformable resouce deadlock");
// 	change(state, &s, batch);
// 	state = s;
// 	return;
//       }
//       if(!firstUse) [[likely]]
// 	batch.mustHappenAfter(providingCurrentState);
//       else
// 	firstUse = false;
//       if(s == state) {
// 	usingCurrentState.push_back(batch);
//       } else {
// 	change(state, &s, batch);
// 	state = s;
// 	for(WorkBatch& consumer : usingCurrentState)
// 	  batch.mustHappenAfter(consumer);
// 	providingCurrentState = batch;
// 	usingCurrentState.clear();
//       }
//     };

//     // //for things like render pass completion that have side-effects on things like layout. Returns old state
//     // S onExternalChange(S s, WorkBatch batch) {
//     //   LOG("Image ? now in layout ", (int)s.layout, " on frame ", WITE::Util::FrameCounter::getFrame());
//     //   Util::ScopeLock l(&lock);
//     //   S old = state;
//     //   if(providingCurrentState == batch) {
//     // 	state = s;
//     // 	return old;
//     //   }
//     //   if(!firstUse) [[likely]]
//     // 	batch.mustHappenAfter(providingCurrentState);
//     //   else
//     // 	firstUse = false;
//     //   if(s == state) {
//     // 	if(!Collections::contains(usingCurrentState, batch))
//     // 	  usingCurrentState.push_back(batch);
//     //   } else {
//     // 	state = s;
//     // 	for(WorkBatch& consumer : usingCurrentState)
//     // 	  batch.mustHappenAfter(consumer);
//     // 	providingCurrentState = batch;
//     // 	usingCurrentState.clear();
//     //   }
//     //   return old;
//     // };

//     S get() {
//       Util::ScopeLock l(&lock);
//       return state;
//     };
//   };

// };
