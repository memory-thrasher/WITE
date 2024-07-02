/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <atomic>
#include <chrono>

#include "wsound.hpp"
#include "DEBUG.hpp"
#include "configuration.hpp"

namespace WITE::wsound {

  SDL_AudioSpec audioFormat, *audioFormatPtr = &audioFormat;
  SDL_AudioDeviceID adev;
  std::atomic_uint64_t framesPlayed;
  std::atomic<float>* combinedSounds;//note: two channel, alternating left and right speaker.
  size_t combinedSoundsSamples;
  constexpr size_t maxContinuousSounds = 10;
  soundCB continuousSounds[maxContinuousSounds];
  std::atomic_size_t continuousSoundCount;
  uint64_t initTime;

  void SDLCALL sdlCallback(void*, Uint8* outRaw, int outBytes) {
    float* out = reinterpret_cast<float*>(outRaw);
    int outSamples = outBytes / sizeof(float);
    uint64_t framesPlayedPreviously = framesPlayed.fetch_add(outSamples/2, std::memory_order_relaxed);
    for(size_t i = 0, c = (framesPlayedPreviously*2)%combinedSoundsSamples;i < outSamples;++i) {
      out[i] = combinedSounds[c].exchange(0, std::memory_order_relaxed);
      ++c;
      if(c >= combinedSoundsSamples)
	c -= combinedSoundsSamples;
    }
    struct outputDescriptor od { framesPlayedPreviously, initTime + framesPlayedPreviously * 1000000000 / audioFormat.freq, audioFormat.freq, reinterpret_cast<sample*>(out), outSamples/2 };
    for(size_t i = 0;i < continuousSoundCount;i++)
      if(continuousSounds[i])
	continuousSounds[i](od);
  };

  constexpr SDL_AudioSpec idealAudioFormat {
    .freq = 44100,
    .format = 0x8120,//little-endian signed 32-bit float
    .channels = 2,
    .samples = 4410,
    .callback = &sdlCallback,
    .userdata = NULL,
  };

  static_assert(sizeof(float) == 4);//using float literal for sample data.
  static_assert((idealAudioFormat.format & 0xFF) == 32);

  uint64_t getInitTimeNs() {
    return initTime;
  };

  void initSound() {
    framesPlayed = 0;
    continuousSoundCount = 0;
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    adev = SDL_OpenAudioDevice(NULL, 0, &idealAudioFormat, &audioFormat, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    ASSERT_TRAP(adev > 0, "failed to create audio device, with error: ", SDL_GetError());
    ASSERT_TRAP(audioFormat.format == idealAudioFormat.format, "unauthorized format change. Asked: ", idealAudioFormat.format, " Actual: ", audioFormat.format);
    combinedSoundsSamples = audioFormat.freq * configuration::getOption("audiobufferlengthms", 10000) / 500;//x/500=x*2/1000
    initTime = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
    SDL_PauseAudioDevice(adev, 0);
    combinedSounds = reinterpret_cast<std::atomic<float>*>(malloc(combinedSoundsSamples * sizeof(float)));
  };

  void pushSound(const sound& s) {
    ASSERT_TRAP(s.length <= combinedSoundsSamples, "pushed sound too long");
    for(size_t i = 0, o = (framesPlayed.load(std::memory_order_relaxed)*2)%combinedSoundsSamples;i < s.length;++i) {
      combinedSounds[o++].fetch_add(s.data[i].left, std::memory_order_relaxed);
      combinedSounds[o++].fetch_add(s.data[i].right, std::memory_order_relaxed);
      if(o >= combinedSoundsSamples/2)
	o -= combinedSoundsSamples/2;
    }
  };

  void addContinuousSound(soundCB scb) {
    size_t idx = continuousSoundCount.fetch_add(1, std::memory_order_relaxed);
    //possible race condition here if co-executed with the audio callback, solved by null check therein
    if(idx >= maxContinuousSounds) [[unlikely]] {
      WARN("skipping continuous sound because the limit of ", maxContinuousSounds, " has been reached");
      continuousSoundCount.fetch_add(-1, std::memory_order_relaxed);
      return;
    }
    continuousSounds[idx] = scb;
  };

  void clearContinuousSounds() {
    //technically this could race, but we don't expect to be calling this and add at the same time, ever
    auto end = continuousSoundCount.exchange(0, std::memory_order_relaxed);
    for(size_t i = 0;i < end;i++)
      continuousSounds[i] = NULL;
  };

}
