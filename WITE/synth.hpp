#include "wsound.hpp"
#include "literalList.hpp"

namespace WITE::wsound {

  constexpr float pi2 = 2*std::numbers::pi_v<float>;
  constexpr size_t periodicMemberCount = 10,
	      rampPhaseCount = 5;//generally 3: attack ramp up, then ramp down to sustained volume, and finally a ramp down at the end

  struct sinusoid {
    //amplitude*sin(freq*2pi*lcv + offset)
    float amplitude, freq, offsetRadians;
  };

  struct periodicMember {
    //see periodicSeries. frequency derived from fundamental and membership index.
    float amplitude, offsetRadians;
  };

  struct periodicSeries {
    //A0 + [𝚺 An*sin(n*fund*2pi*lcv + On)]
    float fundamental;//, average; average (A0) probably not helpful
    literalList<periodicMember> members;
  };

  struct synthParameters {
    float leftVol, rightVol;
    // bool startOn0, stopOn0;//mutates the start/stop time, reducing the play time, so the wave begins/ends with a 0 value to avoid clicks //MAYBE?
  };

  struct rampPhase {
    //startSeconds: negative to specify relative to the end. Ramps may overlap. Generally abs value much less than 1.
    //lengthSeconds: 0 to disable. Generally much less than 1. Never negative.
    //*Volume: relative to unramped volume, multiplied in.
    //curve: difference between desired amplitude and what linear amplitude would be at the midpoint. 0 for linear. Positive for higher amplitude at the midpoint.
    float startSeconds, lengthSeconds = 0, initialVolume, finalVolume, interpolationCurve;
    //MAYBE future extension, if attack noise is desired:
    // periodicMember additionalHarmonics[10];
    // float additionalHarmonicsFundamentalMultiplier, additionalHarmonicsInitialVolume, additionalHarmonicsFinalVolume;
    // rampShape_t additionalHarmonicsShape;
  };

  struct voice {//defines an instrument, not any specific note
    periodicMember members[periodicMemberCount];
    rampPhase ramps[rampPhaseCount];//generally three will be used, but future expansion
  };

  struct note {
    float fundamentalFreq, lengthSeconds;
    uint64_t startTimeNsAfterEpoch;
  };

  void record(const sinusoid& s, const synthParameters& vol, outputDescriptor& out);
  void record(const periodicSeries& s, const synthParameters& vol, outputDescriptor& out);
  void record(const voice& v, const note& n, const synthParameters& vol, outputDescriptor& out);

}
