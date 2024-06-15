#include "synth.hpp"
#include "DEBUG.hpp"

namespace WITE::wsound {

  void record(const sinusoid& s, const synthParameters& vol, outputDescriptor& out) {
    float combinedOffset = fmod(out.startFrame, out.samplingFreq / s.fundMult) + fmod(s.offsetRadians/pi2, 1) / s.fundMult * out.samplingFreq,
      samplerateFreq = pi2 * s.fundMult / out.samplingFreq;
    for(int sample = 0;sample < out.samples;sample++) {
      float v = s.amplitude * sin((sample + combinedOffset) * samplerateFreq);
      out.inout[sample].left += vol.leftVol * v;
      out.inout[sample].right += vol.rightVol * v;
    }
  };

  void record(const voice& instrument, const note& n, const synthParameters& vol, outputDescriptor& out) {
    int noteStartsOnSample = static_cast<int>(static_cast<int64_t>(n.startTimeNsAfterEpoch - out.startTimeNs) *
					      out.samplingFreq / 1000000000);
    int noteSampleLength = n.lengthSeconds * out.samplingFreq;
    int noteSampleEnd = noteSampleLength + noteStartsOnSample;
    for(int sample = std::max(0, noteStartsOnSample);sample < std::min(out.samples, noteSampleEnd);sample++) {
      float v = 0;
      for(const sinusoid& m : instrument.members) {
	//decay rate: [sampleDecayRate]^[sampleRate]=[decayRate]
	if(m.amplitude) [[likely]]
	  v += m.amplitude * (m.decayRate < 1 ? pow(m.decayRate, ((float)(sample - noteStartsOnSample))/out.samplingFreq) : 1) *
	    sin((sample + out.startFrame) * (pi2 * n.fundamentalFreq * m.fundMult / out.samplingFreq) + m.offsetRadians);
      }
      for(const rampPhase& r : instrument.ramps) {
	if(r.lengthSeconds > 0) {
	  float startSample = (r.startSeconds + (r.startSeconds < -0 ? n.lengthSeconds : 0)) *
	    out.samplingFreq + noteStartsOnSample;
	  float endSample = startSample + r.lengthSeconds * out.samplingFreq;
	  if(sample >= startSample && sample < endSample)
	    v *= std::lerp(r.initialVolume, r.finalVolume, (sample - startSample) / (endSample - startSample));
	  //TODO interpolation curve ?
	}
      }
      out.inout[sample].left += vol.leftVol * v;
      out.inout[sample].right += vol.rightVol * v;
    }
  };

}
