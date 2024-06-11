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

// //string or horn:
// inline void randomInstrument(uint64_t in, sound::voice& out) {
//   constexpr uint64_t ampSeed = 31727, offsetSeed = 262153;//prime
//   float remainingAmp = 1;
//   int i = 1;
//   for(sound::periodicMember& m : out.members) {
//     m.amplitude = randFloat(in, ampSeed * i) * remainingAmp * 0.5f;
//     remainingAmp -= m.amplitude;
//     m.offsetRadians = randFloat(in, offsetSeed * i) * sound::pi2;
//     i++;
//   }
//   //TODO more random ramps
//   out.ramps[0] = { 0, 0.01f, 0, 1, 0 };
//   out.ramps[1] = { 0.01f, 0.065f, 1, 0, 0 };
//   out.ramps[2].lengthSeconds = out.ramps[3].lengthSeconds = out.ramps[4].lengthSeconds = 0;
// };

// //WIP percussion
// inline void randomInstrument(uint64_t in, sound::voice& out) {
//   constexpr uint64_t ampSeed = 31727, offsetSeed = 262153;//, decaySeed = 9551;//prime
//   constexpr size_t subs = 256;
//   for(int i = 1;i < subs;i++) {
//     sound::periodicMember& m = out.members[i-1];
//     m.amplitude = randFloat(in, ampSeed * i) * 2 / subs;
//     m.offsetRadians = randFloat(in, offsetSeed * i) * sound::pi2;
//     m.decayRate = i * 0.00001f;
//   }
//   for(int i = subs;i <= sound::periodicMemberCount;i++)
//     out.members[i-1].amplitude = 0;
//   // for(sound::periodicMember& m : out.members)
//   //   WARN(m.amplitude, "\t\t", m.offsetRadians, "\t\t", m.decayRate);
//   // out.ramps[0] = { 0, 0.01f, 0, 1, 0 };
//   // out.ramps[1] = { 0.01f, 0.065f, 1, 0.2f, 0 };
//   // out.ramps[2] = { 0.075f, 0.49f, 0.2f, 0, 0 };
//   // out.ramps[3].lengthSeconds = out.ramps[4].lengthSeconds = 0;
//   for(int i = 0;i < sound::rampPhaseCount;i++)
//     out.ramps[i].lengthSeconds = 0;
// };

// void mkMusic(sound::outputDescriptor& out) {
//   const sound::synthParameters params { 0.3f, 0.3f };//full volume
//   sound::voice instrument;
//   constexpr uint64_t newInstrumentEveryNs = 1000000000;
//   uint64_t startInstrument = out.startTimeNs / newInstrumentEveryNs,
//     endInstrument = (out.startTimeNs + out.samples * 1000000000 / out.samplingFreq) / newInstrumentEveryNs + 1;
//   for(uint64_t instrumentId = startInstrument;instrumentId <= endInstrument;instrumentId++) {
//     randomInstrument(instrumentId, instrument);
//     sound::note note = { 30, 0.5f, instrumentId * newInstrumentEveryNs };
//     // sound::note note = { 123, 0.5f, instrumentId * newInstrumentEveryNs, 123.0f/10, 0.1f };
//     record(instrument, note, params, out);
//   }
// }

void mkMusic(sound::outputDescriptor& out) {
  const sound::synthParameters params { 0.3f, 0.3f };//full volume
  //TODO record with select sinusoids
  static const float segmentVolume = 1.0f;
  static const sound::voice v = {
    {
      { segmentVolume, 1.00f, 1, 16/1000.0f },//0,1
      { segmentVolume, 1.59f, 1, 90/1000.0f },//1,1
      { segmentVolume, 2.14f, 1, 144/1000.0f },//2,1
      { segmentVolume, 2.65f, 1, 179/1000.0f },//3,1
      { segmentVolume, 3.16f, 1, 167/1000.0f },//4,1
      { segmentVolume, 3.65f, 1, 160/1000.0f },//5,1
      { segmentVolume, 4.15f, 1, 157/1000.0f },//6,1
      { segmentVolume, 2.30f, 1, 13/1000.0f },//0,2
      { segmentVolume, 2.92f, 1, 78/1000.0f },//1,2
      { segmentVolume, 3.50f, 1, 36/1000.0f },//2,2
      { segmentVolume, 4.06f, 1, 28/1000.0f },//3,2
      { segmentVolume, 4.15f, 1, 20/1000.0f },//0,3
    }, {
      { 0, 0.01f, 0, 1, 0 },
      { 0.01f, 0.065f, 1, 0.2f, 0 },
      { 0.075f, 0.425f, 0.2f, 0, 0 },
    }
  };
  constexpr uint64_t newInstrumentEveryNs = 1000000000;
  uint64_t startInstrument = out.startTimeNs / newInstrumentEveryNs;
  sound::note n = { 123, 0.5f, startInstrument * newInstrumentEveryNs };
  record(v, n, params, out);
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

