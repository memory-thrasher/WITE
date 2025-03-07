/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "../WITE/WITE.hpp"

struct spawner {
  static constexpr uint64_t typeId = __LINE__;
  static constexpr std::string dbFileId = "spawner";
  uint64_t spawnedCount = 0;
  uint8_t spawnDirection = 0;
  float locationX = 0, locationY = 0, deltaX = 0, deltaY = 0;
  static void update(uint64_t oid, void* db_unused);
};

struct unit {
  static constexpr uint64_t typeId = __LINE__;
  static constexpr std::string dbFileId = "unit";
  static std::atomic_uint64_t updates, allocates, frees, spunUps, spunDowns;
  float locationX = 0, locationY = 0, deltaX = 0, deltaY = 0;
  int ttl;
  static void update(uint64_t oid, void* db_unused);
  static void allocated(uint64_t oid, void* db_unused);
  static void freed(uint64_t oid, void* db_unused);
  static void spunUp(uint64_t oid, void* db_unused);
  static void spunDown(uint64_t oid, void* db_unused);
  typedef std::tuple<float, float> indices_t;
  static indices_t getIndexValues(uint64_t oid, const unit& data, void* db_unused);
};

struct timer {
  static constexpr uint64_t typeId = __LINE__;
  static constexpr std::string dbFileId = "timer";
  static void update(uint64_t oid, void* db_unused);
};

float boxWidth = 1080, boxHeight = 1920;

typedef WITE::database<spawner, unit, timer> db_t;
std::unique_ptr<db_t> db;
std::atomic_bool running;

//having the function definitions out of line might seem odd in this context, but in reality they should be each in their own compilation unit (cpp file).

void spawner::update(uint64_t oid, void* db_unused) {
  spawner s;
  if(!db->readCommitted<spawner>(oid, &s)) return;
  s.locationX += s.deltaX;
  s.locationY += s.deltaY;
  if(s.locationX < 0) {
    s.locationX *= -1;
    s.deltaX *= -1;
  }
  if(s.locationY < 0) {
    s.locationY *= -1;
    s.deltaY *= -1;
  }
  if(s.locationX > boxWidth) {
    s.locationX = 2*boxWidth - s.locationX;
    s.deltaX *= -1;
  }
  if(s.locationY > boxHeight) {
    s.locationY = 2*boxHeight - s.locationY;
    s.deltaY *= -1;
  }
  s.spawnedCount++;
  unit u;
  u.locationX = s.locationX;
  u.locationY = s.locationY;
  u.deltaX = (s.spawnDirection % 3) - 1;
  u.deltaY = (s.spawnDirection / 3) - 1;
  u.ttl = 120;
  s.spawnDirection = (s.spawnDirection + 1) % 9;
  db->create(&u);
  db->write<spawner>(oid, &s);
};

std::atomic_uint64_t unit::updates, unit::allocates, unit::frees, unit::spunUps, unit::spunDowns;

void unit::update(uint64_t oid, void* db_unused) {
  unit s;
  updates++;
  if(!db->readCommitted<unit>(oid, &s)) return;
  { //test lookup that might have multiple hits, must include this one
    ASSERT_TRAP((db->template findByIdx<unit, 0>(s.locationX) != WITE::NONE), "exact match not found but should at least find this one");
    bool found = false;
    db->foreachByIdx<unit, 1>(s.locationY, [&found, oid](float, uint64_t ooid) {
      if(ooid == oid) [[unlikely]]
	found = true;
    });
    ASSERT_TRAP(found, "could not find this object in the index of objects by locationY with this object's Y");
  }
  if(s.ttl-- < 0) {
    db->destroy<unit>(oid);
    return;
  }
  s.locationX += s.deltaX;
  s.locationY += s.deltaY;
  if(s.locationX < 0) {
    s.locationX *= -1;
    s.deltaX *= -1;
  }
  if(s.locationY < 0) {
    s.locationY *= -1;
    s.deltaY *= -1;
  }
  if(s.locationX > boxWidth) {
    s.locationX = 2*boxWidth - s.locationX;
    s.deltaX *= -1;
  }
  if(s.locationY > boxHeight) {
    s.locationY = 2*boxHeight - s.locationY;
    s.deltaY *= -1;
  }
  db->write<unit>(oid, &s);
};

void unit::allocated(uint64_t oid, void* db_unused) {
  allocates++;
};

void unit::freed(uint64_t oid, void* db_unused) {
  frees++;
};

void unit::spunUp(uint64_t oid, void* db_unused) {
  spunUps++;
};

void unit::spunDown(uint64_t oid, void* db_unused) {
  spunDowns++;
};

unit::indices_t unit::getIndexValues(uint64_t oid, const unit& data, void*) {
  return std::tie(data.locationX, data.locationY);
};

void timer::update(uint64_t oid, void* db_unused) {
  timer s;
  if(!db->readCommitted<timer>(oid, &s)) return;
  if(db->getFrame() > 500)
    running = false;
  db->write<timer>(oid, &s);
};

int main(int argc, const char** argv) {
  WITE::configuration::setOptions(argc, argv);
  std::filesystem::path dirPath = std::filesystem::temp_directory_path() / "wite_db_test";
  db = std::make_unique<db_t>(dirPath.string(), true, true);//blow away any existing file
  //create some objects before entering the game loop
  spawner s;
  s.locationX = s.locationY = 10;
  s.deltaX = s.deltaY = 1;
  db->create<spawner>(&s);
  timer t;
  db->create<timer>(&t);
  //game loop
  running = true;
  while(running) {
    db->updateTick();
    db->endFrame();
  }
  db->gracefulShutdown();
  db->deleteFiles();
  std::cout << "updates: " << unit::updates << " allocates: " << unit::allocates << " frees: " << unit::frees << " spunUps: " << unit::spunUps << " spunDowns: " << unit::spunDowns << "\n";
  db.reset();
};

