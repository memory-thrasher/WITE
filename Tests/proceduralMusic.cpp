#include <numbers>
#include <iostream>

#include "../WITE/WITE.hpp"

namespace sound = WITE::wsound;

/*
this synth is designed to produce music that sounds both alien and melodic
discarded human traditions replaced with equally arbitrary conventions: notes per octive, typical beat frequency, typical note length, instruments
kept human traditions: concept of certain instruments (string, horn, membrain drum), size of octive (doubled frequency), size of a second (used for frequency calulations only, not rhythm)
 */

constexpr uint64_t arbitraryAlienTradition = 15,
	    beatsPerMeasure = arbitraryAlienTradition,
	    beatLengthNs = 54568889,
	    measureNs = beatsPerMeasure * beatLengthNs,
	    notesPerOctive = arbitraryAlienTradition,
	    octives = 8,
	    notes = octives * notesPerOctive;

constexpr float firstNote = arbitraryAlienTradition;//hz

const float noteBase = pow(2, 1.0f/notesPerOctive);//would be constexpr if pow was (c++2026 draft feature)

struct track {
  uint8_t noteId[notesPerMeasure];
  uint8_t noteLengthHalfBeats;//applies to all notes in this track
  bool melody;//true: notes play in series; false: notes play all at once (it's a chord)
  sound::voice voice;
};

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

//string or horn:
inline void randomPeriodicInstrument(uint64_t in, sound::voice& out) {
  constexpr uint64_t ampSeed = 31727, offsetSeed = 262153;//prime
  float remainingAmp = 1;
  int i = 1;
  for(sound::sinusoid& m : out.members) {
    m.amplitude = randFloat(in, ampSeed * i) * remainingAmp * 0.5f;
    remainingAmp -= m.amplitude;
    m.offsetRadians = randFloat(in, offsetSeed * i) * sound::pi2;
    i++;
  }
  out.ramps[0] = { 0, 0.01f, 0, 1, 0 };
  out.ramps[1] = { -0.05f, 0.05f, 1, 0, 0 };
  out.ramps[2].lengthSeconds = out.ramps[3].lengthSeconds = out.ramps[4].lengthSeconds = 0;
};

inline void randomPercussionInstrument(uint64_t in, sound::voice& out) {
  constexpr uint64_t ampSeed = 31727,
    // offsetSeed = 262153,
    decaySeed = 9551,
    slopeSeed = 10900063;//prime
  const float majorPitch = randFloat(in, slopeSeed) + 1.0f;
  for(int major = 1;major <= 4;major++) {
    const float minorPitch = randFloat(in, slopeSeed * (major+1)) * 0.5f + 0.25f;
    for(int minor = 0;minor < 8;minor++) {
      int i = (major-1)*8+minor;
      ASSERT_TRAP(i < sound::sinusoidCount, "index out of range");
      sound::sinusoid& m = out.members[i];
      m.amplitude = randFloat(in, ampSeed * i) * 0.5f / major;
      m.fundMult = 1 + majorPitch * (major - 1) + minorPitch * minor;
      m.offsetRadians = 0;//randFloat(in, offsetSeed * i) * sound::pi2;
      m.decayRate = (randFloat(in, decaySeed * i) + 1) / (100 * major);
    }
  }
  out.ramps[0] = { 0, 0.01f, 0, 1, 0 };
  out.ramps[1] = { 0.01f, 0.065f, 1, 0.2f, 0 };
  out.ramps[2] = { 0.075f, 0.425f, 0.2f, 0, 0 };
  out.ramps[3].lengthSeconds = out.ramps[4].lengthSeconds = 0;
};

inline void randomTrack(uint64_t id, track& out) {
  //in keeping with the arbitrarilly chosen alien emulating theme of 15-based numbers, there are always 15 simultaneous tracks. 5 drums (melodic) tracks, 5 melodic periodic tracks, 5 corded periodic tracks
  //we don't want notes that are 17/30th or other obtuse values in lengths. Limit to [1|2|4|6|10|15|30]/30. Further limited by track type below.
  static constexpr uint64_t lengthPrime = 800009779,
    maxLength = beatsPerMeasure*2;
  int type = id % 3;
  uint8_t minNote = 0, maxNote = 0;
  switch(type) {
  case 0:
    randomPercussionInstrument(id, out.voice);
    out.melody = true;
    out.noteLengthHalfBeats = maxLength;//this means little for percussion, just give it time to peter out on its own decay model
    //
    break;
  case 1:
    randomPeriodicInstrument(id, out.voice);
    out.melody = true;
    static constexpr uint8_t possibleLengths[] { 1, 2, 4 };
    out.noteLengthHalfBeats = TODO;//
    break;
  case 2:
    randomPeriodicInstrument(id, out.voice);
    out.melody = false;//chord
    static constexpr uint8_t possibleLengths[] { 6, 10, 15, 30 };
    //
    break;
};

inline void randomNote(uint8_t alienOctive, sound::voice& out) {
};

void mkMusic(sound::outputDescriptor& out) {
  const sound::synthParameters params { 0.3f, 0.3f };//full volume
  sound::voice instrument;
  constexpr uint64_t newInstrumentEveryNs = 1000000000;
  uint64_t startInstrument = out.startTimeNs / newInstrumentEveryNs,
    endInstrument = (out.startTimeNs + out.samples * 1000000000 / out.samplingFreq) / newInstrumentEveryNs + 1;
  for(uint64_t instrumentId = startInstrument;instrumentId <= endInstrument;instrumentId++) {
    randomInstrument(instrumentId, instrument);
    sound::note note = { 30, 0.5f, instrumentId * newInstrumentEveryNs };
    // sound::note note = { 123, 0.5f, instrumentId * newInstrumentEveryNs, 123.0f/10, 0.1f };
    record(instrument, note, params, out);
  }
}

// void mkMusic(sound::outputDescriptor& out) {
//   const sound::synthParameters params { 0.3f, 0.3f };//full volume
//   sound::voice v;
//   constexpr uint64_t newInstrumentEveryNs = 1000000000;
//   uint64_t startInstrument = out.startTimeNs / newInstrumentEveryNs;
//   randomPercussionInstrument(startInstrument, v);
//   sound::note n = { 120, 0.5f, startInstrument * newInstrumentEveryNs };
//   record(v, n, params, out);
// }

int main(int argc, char** argv) {
  sound::initSound();
  sound::addContinuousSound(sound::soundCB_F::make(mkMusic));
  WITE::winput::initInput();
  for(size_t i = 0;i < 4*5 && !WITE::shutdownRequested();i++) {
    WITE::winput::pollInput();
    WITE::thread::sleepSeconds(0.25f);
  }
  return 0;
}

