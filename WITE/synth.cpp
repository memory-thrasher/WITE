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

  void record(const voice& instrument, const note& n, const synthParameters& vol, outputDescriptor& out) {
    float startFrameWrapped = fmod(out.startFrame, out.samplingFreq / n.fundamentalFreq);//subharmonics will not mind this modulus because they are multiples of that divisor. This improves precision on the float conversion below by saving significant bits for the subsecond parts.
    int noteStartsOnSample = static_cast<int>(static_cast<int64_t>(n.startTimeNsAfterEpoch - out.startTimeNs) *
					      out.samplingFreq / 1000000000);
    int noteSampleLength = n.lengthSeconds * out.samplingFreq;
    int noteSampleEnd = noteSampleLength + noteStartsOnSample;
    for(int sample = std::max(0, noteStartsOnSample);sample < std::min(out.samples, noteSampleEnd);sample++) {
      float v = 0;
      int i = 0;
      for(const periodicMember& m : instrument.members)
	v += m.amplitude * sin((sample + startFrameWrapped) * (pi2 * n.fundamentalFreq * ++i / out.samplingFreq) + m.offsetRadians);
      for(const rampPhase& r : instrument.ramps) {
	if(r.lengthSeconds > 0) {
	  float startSample = (r.startSeconds + (r.startSeconds < 0 ? r.lengthSeconds : 0)) *
	    out.samplingFreq + noteStartsOnSample;
	  float endSample = startSample + r.lengthSeconds * out.samplingFreq;
	  if(sample > startSample && sample < endSample)
	    v *= std::lerp(r.initialVolume, r.finalVolume, (sample - startSample) / (endSample - startSample));
	  //TODO interpolation curve ?
	}
      }
      out.inout[sample].left += vol.leftVol * v;
      out.inout[sample].right += vol.rightVol * v;
    }
  };

}
