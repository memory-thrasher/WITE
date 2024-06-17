/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "wsound.hpp"
#include "literalList.hpp"

namespace WITE::wsound {

  constexpr float pi2 = 2*std::numbers::pi_v<float>;
  constexpr size_t sinusoidCount = 32,
	      rampPhaseCount = 5;

  struct sinusoid {
    float amplitude = 0, fundMult, offsetRadians, decayRate = 1;//decayRate = multiple per second, 1 => no decay
  };

  struct synthParameters {
    float leftVol, rightVol;
  };

  struct rampPhase {
    //startSeconds: negative to specify relative to the end. Ramps may overlap. Generally abs value much less than 1.
    //lengthSeconds: 0 to disable. Generally much less than 1. Never negative.
    //*Volume: relative to unramped volume, multiplied in.
    //curve: difference between desired amplitude and what linear amplitude would be at the midpoint. 0 for linear. Positive for higher amplitude at the midpoint.
    float startSeconds, lengthSeconds = 0, initialVolume, finalVolume, interpolationCurve;
  };

  struct voice {//defines an instrument, not any specific note
    sinusoid members[sinusoidCount];
    rampPhase ramps[rampPhaseCount];
  };

  struct note {
    float fundamentalFreq, lengthSeconds;
    uint64_t startTimeNsAfterEpoch;
    // float blanketFundamental, blanketStrength = 0;
  };

  void record(const sinusoid& s, const synthParameters& vol, outputDescriptor& out);//NOTE: does not decay
  void record(const voice& v, const note& n, const synthParameters& vol, outputDescriptor& out);

}
