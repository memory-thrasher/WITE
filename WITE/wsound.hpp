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

  typedefCB(soundCB, void, uint64_t, int, sample*, int);//uint64_t startSampleFrame, int freq, sample* inout, int outlen //new data should be mixed in

  void initSound();
  inline const SDL_AudioSpec& getAudioFormat() { return *audioFormatPtr; };
  void pushSound(const sound&);
  void addContinuousSound(soundCB);//for bg music and similar
  void clearContinuousSounds();

}
