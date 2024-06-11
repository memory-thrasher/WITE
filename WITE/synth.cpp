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
    float startFrameWrapped = fmod(out.startFrame, out.samplingFreq / n.fundamentalFreq);//subharmonics will not mind this modulus because they are multiples of that divisor. This improves precision on the float conversion below by saving significant bits for the subsecond parts.
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
	    sin((sample + startFrameWrapped) * (pi2 * n.fundamentalFreq * m.fundMult / out.samplingFreq) + m.offsetRadians);
	// if(sample == out.samples-1)
	//   WARN(m.decayRate, " -> ", (m.decayRate < 1 ? pow(m.decayRate, ((float)sample)/out.samplingFreq) : 1));
      }
      // if(n.blanketStrength > 0) {
      // 	float blankedStartFrameWrapped = fmod(out.startFrame, out.samplingFreq / n.blanketFundamental);
      // 	int blanketCount = (int)(out.samplingFreq / (4*n.blanketFundamental));
      // 	for(int i = 0;i < blanketCount;i++)
      // 	  v += (n.blanketStrength * 2 / blanketCount) * sin((sample + blankedStartFrameWrapped) * (pi2 * n.blanketFundamental * i / out.samplingFreq));
      // }
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
