#include <numbers>
#include <iostream>

#include "../WITE/WITE.hpp"

namespace sound = WITE::wsound;

uint64_t dualSeedRand(uint64_t in, uint64_t in2) {
  static constexpr uint64_t primes[] = { 23339, 93719, 16788263, 4294988419ull, 1099511633629ull,
    281474976738719ull, 72057594037951409ull };
  uint64_t ret = in, fold = in2;
  for(const uint64_t& p : primes) {
    ret += (in * p)^fold;
    fold += in2 * p;
  }
  return ret;
};

inline float randFloat(uint64_t in, uint64_t in2) {//[0-1)
  constexpr uint64_t resolutionBits = 16;
  return fmod(static_cast<float>(dualSeedRand(in, in2) % (1ull << 32)) / (1ull << resolutionBits), 1);
};

inline void randomInstrument(uint64_t in, sound::voice& out) {
  constexpr uint64_t ampSeed = 31727, offsetSeed = 262153;//prime
  float remainingAmp = 1;
  int i = 1;
  for(sound::periodicMember& m : out.members) {
    m.amplitude = randFloat(in, ampSeed * i) * remainingAmp * 0.5f;
    remainingAmp -= m.amplitude;
    m.offsetRadians = randFloat(in, offsetSeed * i) * sound::pi2;
    i++;
  }
  //TODO more random ramps
  out.ramps[0] = { 0, 0.02f, 0, 1, 0 };
  out.ramps[1] = { -0.1f, 0.1f, 1, 0, 0 };
  out.ramps[2].lengthSeconds = out.ramps[3].lengthSeconds = out.ramps[4].lengthSeconds = 0;
};

void mkMusic(sound::outputDescriptor& out) {
  const sound::synthParameters params { 0.3f, 0.3f };//full volume
  sound::voice instrument;
  constexpr uint64_t newInstrumentEveryNs = 250000000;
  uint64_t startInstrument = out.startTimeNs / newInstrumentEveryNs,
    endInstrument = (out.startTimeNs + out.samples * 1000000000 / out.samplingFreq) / newInstrumentEveryNs;
  for(uint64_t instrumentId = startInstrument;instrumentId <= endInstrument;instrumentId++) {
    randomInstrument(instrumentId, instrument);
    sound::note note = { 440, 1, startInstrument * newInstrumentEveryNs };
    record(instrument, note, params, out);
  }
}

int main(int argc, char** argv) {
  sound::initSound();
  sound::addContinuousSound(sound::soundCB_F::make(mkMusic));
  WITE::winput::initInput();
  for(size_t i = 0;i < 4*15 && !WITE::shutdownRequested();i++) {
    WITE::winput::pollInput();
    WITE::thread::sleepSeconds(0.25f);
  }
  return 0;
}

