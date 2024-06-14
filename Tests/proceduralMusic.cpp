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
	    notesPerMeasure = arbitraryAlienTradition,
	    beatLengthNs = 54568889,
	    measureNs = notesPerMeasure * beatLengthNs,
	    concurrentTracks = 5,
	    newTrackEveryMeasures = 3,
	    newTrackEveryNs = newTrackEveryMeasures * measureNs,
	    trackLifetimeMeasures = concurrentTracks * newTrackEveryMeasures,
	  //	    trackLifetimeNs = trackLifetimeMeasures * measureNs,
	    notesPerOctive = arbitraryAlienTradition,
	    octives = 6;
	  //	    notes = octives * notesPerOctive;

constexpr float firstNote = 30,//hz
	    globalVolumeMultiplier = 1.0f/concurrentTracks;

const float noteBase = pow(2, 1.0f/notesPerOctive);//would be constexpr if pow was (c++2026 draft feature)

struct track {
  uint8_t noteId[notesPerMeasure];//0 = silence
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
  float remainingAmp = globalVolumeMultiplier;
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
      m.amplitude = randFloat(in, ampSeed * i) * globalVolumeMultiplier / major;
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
  //we don't want notes that are 17/30th or other obtuse values in lengths. Limit to factors of 30. Further limited by track type below.
  static constexpr uint64_t lengthPrime = 800009779,
    notePrime = 17957;
  //1, 2, 3, 5, 6, 10, 15, 30
  // static constexpr uint8_t possibleLengthsMelody[] = { 1, 2, 3, 6, 6, 6, 6, 3, 3, 2 },
  static constexpr uint8_t possibleLengthsMelody[] = { 5, 6, 10, 15, 6, 10, 10, 10, 15 },
    // possibleLengthsChord[] = { 6, 10, 15, 30 },
    possibleLengthsPercussion[] = { 10, 15, 30 };
  uint8_t minBaseNote = 0, maxBaseNote = 0, possibleLengthsCount;
  const uint8_t *possibleLengths;
  switch(id % concurrentTracks) {
  case 0:
  case concurrentTracks/2:
    randomPercussionInstrument(id, out.voice);
    out.melody = true;
    possibleLengthsCount = sizeof(possibleLengthsPercussion);
    possibleLengths = possibleLengthsPercussion;
    minBaseNote = 0;
    maxBaseNote = notesPerOctive;
    break;
  default:
    randomPeriodicInstrument(id, out.voice);
    out.melody = true;
    possibleLengthsCount = sizeof(possibleLengthsMelody);
    possibleLengths = possibleLengthsMelody;
    minBaseNote = notesPerOctive * 2;
    maxBaseNote = notesPerOctive * octives;
    break;
  // case concurrentTracks/2:
  //   randomPeriodicInstrument(id, out.voice);
  //   out.melody = false;//chord
  //   possibleLengthsCount = sizeof(possibleLengthsChord);
  //   possibleLengths = possibleLengthsChord;
  //   minBaseNote = 0;
  //   maxBaseNote = notesPerOctive * 2;
  //   break;
  }
  out.noteLengthHalfBeats = possibleLengths[dualSeedRand(id, lengthPrime) % possibleLengthsCount];
  const uint8_t stride = (out.noteLengthHalfBeats + 1)/2;
  const uint8_t baseNote = minBaseNote + dualSeedRand(id, notePrime) % (maxBaseNote - minBaseNote);
  for(uint8_t i = 0;i < notesPerMeasure;++i) {
    out.noteId[i] = (i % stride) == 0 ? dualSeedRand(id, notePrime * (i + 11)) % notesPerOctive : 0;
    if(out.noteId[i] != 0) [[likely]] out.noteId[i] += baseNote;
  }
};

void mkMusic(sound::outputDescriptor& out) {
  const sound::synthParameters params { 0.3f, 0.3f };//full volume
  const uint64_t endInstrument = (out.startTimeNs + out.samples * 1000000000 / out.samplingFreq) / newTrackEveryNs + 1,
    startInstrument = out.startTimeNs / newTrackEveryNs - concurrentTracks,
    firstMeasureId = out.startTimeNs / measureNs,
    lastMeasureId = (out.startTimeNs + out.samples * 1000000000 / out.samplingFreq) / measureNs + 1;
  // WARN("tracks present on this frame: ", endInstrument - startInstrument + 1);
  sound::note note;
  track t;
  for(uint64_t instrumentId = startInstrument;instrumentId <= endInstrument;++instrumentId) {
    randomTrack(instrumentId, t);
    note.lengthSeconds = (t.noteLengthHalfBeats * beatLengthNs) / 1000000000.0f;
    for(int noteId = 0;noteId < notesPerMeasure;++noteId) {
      if(t.noteId[noteId] == 0) [[unlikely]] continue;
      note.fundamentalFreq = firstNote * pow(noteBase, t.noteId[noteId]);
      uint64_t firstMeasureForTrack = instrumentId * newTrackEveryMeasures,
	lastMeasureForTrack = firstMeasureForTrack + trackLifetimeMeasures;
      for(uint64_t measureId = std::max(firstMeasureId, firstMeasureForTrack);measureId <= std::min(lastMeasureId, lastMeasureForTrack);++measureId) {
	note.startTimeNsAfterEpoch = measureId * measureNs + noteId * t.melody * beatLengthNs;
	record(t.voice, note, params, out);
      }
    }
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
  // for(size_t i = 0;i < 60*1 && !WITE::shutdownRequested();i++) {
  while(!WITE::shutdownRequested()) {
    WITE::winput::pollInput();
    WITE::thread::sleepSeconds(0.1f);
  }
  return 0;
}

