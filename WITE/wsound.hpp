/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "SDL.hpp"
#include "callback.hpp"

namespace WITE::wsound {

  extern SDL_AudioSpec *audioFormatPtr;

  struct sample {
    float left, right;
  };

  struct sound {
    const size_t length;
    const sample* data;
  };

  struct outputDescriptor {
    uint64_t startFrame, startTimeNs;
    int samplingFreq;
    sample* inout;
    int samples;
  };

  typedefCB(soundCB, void, outputDescriptor&);

  void initSound();
  inline const SDL_AudioSpec& getAudioFormat() { return *audioFormatPtr; };
  void pushSound(const sound&);
  void addContinuousSound(soundCB);//for bg music and similar
  void clearContinuousSounds();
  uint64_t getInitTimeNs();//wraparound is in December, 2554. If you're reading this in 2555, greetings from 2024.

}
