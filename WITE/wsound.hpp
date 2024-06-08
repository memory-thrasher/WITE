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
