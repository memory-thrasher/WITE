#include "wsound.hpp"
#include "literalList.hpp"

namespace WITE::wsound {

  constexpr float pi2 = 2*std::numbers::pi_v<float>;

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

  struct voice {//defines an instrument, not any specific note
    periodicMember members[10];
  };

  void record(const sinusoid& s, const synthParameters& vol, outputDescriptor& out);
  void record(const periodicSeries& s, const synthParameters& vol, outputDescriptor& out);

}
