#include <numbers>
#include <iostream>

#include "../WITE/WITE.hpp"

namespace sound = WITE::wsound;

void mkMusic(uint64_t startFrame, int freq, sound::sample* inout, int samples) {
  for(int i = 0;i < samples;i++) {
    float v = sin((i + startFrame) * (2*std::numbers::pi_v<float>*440.0f / freq)) * 0.3f;
    inout[i].left += v;
    inout[i].right += v;
  }
}

int main(int argc, char** argv) {
  sound::initSound();
  sound::addContinuousSound(sound::soundCB_F::make(mkMusic));
  WITE::thread::sleepSeconds(10);
  return 0;
}

