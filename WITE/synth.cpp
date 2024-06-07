#include "synth.hpp"

namespace WITE::wsound {

  void record(const sinusoid& s, const synthParameters& vol, outputDescriptor& out) {
    float combinedOffset = fmod(out.startFrame, out.samplingFreq / s.freq) + fmod(s.offsetRadians/pi2, 1) / s.freq * out.samplingFreq,
      samplerateFreq = pi2 * s.freq / out.samplingFreq;
    for(int sample = 0;sample < out.samples;sample++) {
      float v = s.amplitude * sin((sample + combinedOffset) * samplerateFreq);
      out.inout[sample].left += vol.leftVol * v;
      out.inout[sample].right += vol.rightVol * v;
    }
  };

  void record(const periodicSeries& s, const synthParameters& vol, outputDescriptor& out) {
    int i = 1;
    sinusoid si;
    for(const periodicMember& m : s.members) {
      si.amplitude = m.amplitude;
      si.freq = i * s.fundamental;
      si.offsetRadians = m.offsetRadians;
      record(si, vol, out);
      i++;
    }
  };

}
