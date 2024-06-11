#include "wsound.hpp"
#include "literalList.hpp"

namespace WITE::wsound {

  constexpr float pi2 = 2*std::numbers::pi_v<float>;
  constexpr size_t sinusoidCount = 32,
	      rampPhaseCount = 5;//generally 3: attack ramp up, then ramp down to sustained volume, and finally a ramp down at the end

  struct sinusoid {
    //amplitude*sin(freq*2pi*lcv + offset)
    float amplitude = 0, fundMult, offsetRadians, decayRate = 1;//decayRate = multiple per second, 1 => no decay
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
    // sinusoid additionalHarmonics[sinusoidCount];
    // float additionalHarmonicsFundamentalMultiplier, additionalHarmonicsInitialVolume, additionalHarmonicsFinalVolume;
    // rampShape_t additionalHarmonicsShape;
  };

  struct voice {//defines an instrument, not any specific note
    sinusoid members[sinusoidCount];
    rampPhase ramps[rampPhaseCount];//generally three will be used, but future expansion
  };

  struct note {
    float fundamentalFreq, lengthSeconds;
    uint64_t startTimeNsAfterEpoch;
    // float blanketFundamental, blanketStrength = 0;
  };

  void record(const sinusoid& s, const synthParameters& vol, outputDescriptor& out);//NOTE: does not decay
  void record(const voice& v, const note& n, const synthParameters& vol, outputDescriptor& out);

}
