/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "synth.hpp"
#include "DEBUG.hpp"
#include "math.hpp"

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
    float offsetBySinusoid[sinusoidCount];
    float periodBySinusoid[sinusoidCount];
    for(int i = 0;i < sinusoidCount;i++) {
      const sinusoid& m = instrument.members[i];
      if(m.amplitude) [[likely]] {
	periodBySinusoid[i] = n.fundamentalFreq * m.fundMult / out.samplingFreq;
	offsetBySinusoid[i] = intModFloat(out.startFrame, 1 / periodBySinusoid[i]);
      }
    }
    for(int sample = std::max(0, noteStartsOnSample);sample < std::min(out.samples, noteSampleEnd);sample++) {
      float v = 0;
      for(int i = 0;i < sinusoidCount;i++) {
	const sinusoid& m = instrument.members[i];
	//decay rate: [sampleDecayRate]^[sampleRate]=[decayRate]
	if(m.amplitude) [[likely]]
	  v += m.amplitude * (m.decayRate < 1 ? pow(m.decayRate, ((float)(sample - noteStartsOnSample))/out.samplingFreq) : 1) *
	    sin((sample + offsetBySinusoid[i]) * (pi2 * periodBySinusoid[i]) + m.offsetRadians);
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
