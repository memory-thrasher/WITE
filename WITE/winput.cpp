/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <map>

#include "concurrentReadSyncLock.hpp"
#include "SDL.hpp"
#include "winput.hpp"
#include "shutdown.hpp"
#include "DEBUG.hpp"

namespace WITE::winput {

  std::map<inputIdentifier, compositeInputData> allInputData;
  concurrentReadSyncLock allInputData_mutex;

  void handleEvent(uint32_t timestamp, const inputIdentifier& ii, float x = NAN, float y = NAN, float z = NAN) {
    compositeInputData& cid = allInputData[ii];
    uint32_t oldDelta = cid.lastTime - cid.firstTime;
    float axes[3] = { x, y, z };
    for(int i = 0;i < 3;i++) {
      float newValue = axes[i];
      if(std::isnan(axes[i])) [[likely]] //NAN
	newValue = cid.axes[i].current;
      cid.axes[i].delta += newValue - cid.axes[i].current;
      if(newValue != cid.axes[i].current) {
	if(newValue > 0)
	  cid.axes[i].numPositive++;
	else if(newValue < -0)
	  cid.axes[i].numNegative++;
	else
	  cid.axes[i].numZero++;
      }
      if(oldDelta) [[likely]] {
	uint32_t newDelta = timestamp - cid.lastTime;
	cid.axes[i].average = (cid.axes[i].average * oldDelta + cid.axes[i].current * newDelta) / (newDelta + oldDelta);
	if(newValue < cid.axes[i].min)
	  cid.axes[i].min = newValue;
	if(newValue > cid.axes[i].max)
	  cid.axes[i].max = newValue;
      } else {
	cid.axes[i].average = cid.axes[i].current;
	cid.axes[i].min = cid.axes[i].max = newValue;
      }
      cid.axes[i].current = newValue;
    }
    cid.lastTime = timestamp;
  };

  int processEvent(void*, SDL_Event* event) {
    switch(event->type) {
    case SDL_QUIT:
      requestShutdown();
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      static_assert(sizeof(event->key.keysym.sym) == sizeof(uint32_t));//sdl lib uses signed int, with negatives being invalid
      handleEvent(event->common.timestamp, { type_e::key, 0, static_cast<uint32_t>(event->key.keysym.sym) }, event->key.state == SDL_PRESSED ? 1 : 0);
      break;
    case SDL_MOUSEWHEEL:
      handleEvent(event->common.timestamp, { type_e::mouseWheel, event->wheel.which, 0 }, event->wheel.preciseX, event->wheel.preciseY);
      handleEvent(event->common.timestamp, { type_e::mouseWheel, event->wheel.which, 0 }, 0, 0);//treat wheel as a button that is pressed then released, never held
      break;
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEBUTTONDOWN:
      handleEvent(event->common.timestamp, { type_e::mouseButton, event->button.which, event->button.button },
		  event->button.state == SDL_PRESSED ? 1 : 0, event->button.clicks);
      break;
    case SDL_MOUSEMOTION:
      handleEvent(event->common.timestamp, { type_e::mouse, event->motion.which, 0 }, event->motion.x, event->motion.y);
      break;
    // case SDL_CONTROLLERSENSORUPDATE:
    //   handleEvent(event->common.timestamp, TODO);
    //   break;
    // case SDL_CONTROLLERTOUCHPADUP:
    //   handleEvent(event->common.timestamp, TODO);
    //   break;
    // case SDL_CONTROLLERTOUCHPADMOTION:
    //   handleEvent(event->common.timestamp, TODO);
    //   break;
    // case SDL_CONTROLLERTOUCHPADDOWN:
    //   handleEvent(event->common.timestamp, TODO);
    //   break;
    // case SDL_CONTROLLERDEVICEREMAPPED:
    //   handleEvent(event->common.timestamp, TODO);
    //   break;
    // case SDL_CONTROLLERDEVICEREMOVED:
    //   handleEvent(event->common.timestamp, TODO);
    //   break;
    // case SDL_CONTROLLERDEVICEADDED:
    //   handleEvent(event->common.timestamp, TODO);
    //   break;
    // case SDL_JOYDEVICEREMOVED:
    //   handleEvent(event->common.timestamp, TODO);
    //   break;
    // case SDL_JOYDEVICEADDED:
    //   handleEvent(event->common.timestamp, TODO);
    //   break;
    case SDL_CONTROLLERBUTTONUP:
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_JOYBUTTONUP://joy and controller structs are identical
    case SDL_JOYBUTTONDOWN:
      static_assert(sizeof(event->jbutton.which) == sizeof(uint32_t));//sdl lib uses signed int, with negatives being invalid
      handleEvent(event->common.timestamp, { type_e::joyButton, static_cast<uint32_t>(event->jbutton.which), event->jbutton.button },
		  event->jbutton.state == SDL_PRESSED ? 1 : 0);
      break;
    case SDL_CONTROLLERAXISMOTION://joy and controller structs are identical
    case SDL_JOYAXISMOTION:
      static_assert(sizeof(event->jaxis.which) == sizeof(uint32_t));//sdl lib uses signed int, with negatives being invalid
      handleEvent(event->common.timestamp, { type_e::joyAxis, static_cast<uint32_t>(event->jaxis.which), event->jaxis.axis }, event->jaxis.value / 32768.0f);
      break;
    case SDL_JOYHATMOTION: //a hat is 4 buttons
      static_assert(sizeof(event->jhat.which) == sizeof(uint32_t));//sdl lib uses signed int, with negatives being invalid
      static_assert(sizeof(event->jhat.hat) == 1);
      handleEvent(event->common.timestamp, { type_e::joyButton, static_cast<uint32_t>(event->jhat.which),
					    (static_cast<uint32_t>(event->jhat.hat) << 8) + 0 },
	(event->jhat.value & SDL_HAT_UP) != 0 ? 1 : 0);
      handleEvent(event->common.timestamp, { type_e::joyButton, static_cast<uint32_t>(event->jhat.which),
					    (static_cast<uint32_t>(event->jhat.hat) << 8) + 1 },
	(event->jhat.value & SDL_HAT_DOWN) != 0 ? 1 : 0);
      handleEvent(event->common.timestamp, { type_e::joyButton, static_cast<uint32_t>(event->jhat.which),
					    (static_cast<uint32_t>(event->jhat.hat) << 8) + 2 },
	(event->jhat.value & SDL_HAT_LEFT) != 0 ? 1 : 0);
      handleEvent(event->common.timestamp, { type_e::joyButton, static_cast<uint32_t>(event->jhat.which),
					    (static_cast<uint32_t>(event->jhat.hat) << 8) + 3 },
	(event->jhat.value & SDL_HAT_RIGHT) != 0 ? 1 : 0);
      break;
    case SDL_JOYBALLMOTION: //a ball is 2 axes //do these even exist?
      handleEvent(event->common.timestamp, { type_e::joyAxis, static_cast<uint32_t>(event->jball.which),
					    (static_cast<uint32_t>(event->jball.ball) << 8) },
	event->jball.xrel / 32768.0f);
      handleEvent(event->common.timestamp, { type_e::joyAxis, static_cast<uint32_t>(event->jball.which),
					    (static_cast<uint32_t>(event->jball.ball) << 8) + 1 },
	event->jball.yrel / 32768.0f);
      break;
    // case SDL_WINDOWEVENT:
    //   WARN("window ", event->window.windowID, " event of subtype ", (int)event->window.event);
    //   break;
    default:
      // WARN("read event of unknown type ", event->type);
      break;
    }
    return 1;
  };

  void initInput() {//static
    static constexpr uint32_t enabledEvents[] = {
      //SDL_WINDOWEVENT,
      SDL_KEYDOWN, SDL_KEYUP,
      SDL_MOUSEWHEEL, SDL_MOUSEBUTTONUP, SDL_MOUSEBUTTONDOWN, SDL_MOUSEMOTION,
      SDL_CONTROLLERSENSORUPDATE, SDL_CONTROLLERTOUCHPADUP, SDL_CONTROLLERTOUCHPADMOTION, SDL_CONTROLLERTOUCHPADDOWN, SDL_CONTROLLERDEVICEREMAPPED, SDL_CONTROLLERDEVICEREMOVED, SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERBUTTONUP, SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERAXISMOTION,
      SDL_JOYDEVICEREMOVED, SDL_JOYDEVICEADDED, SDL_JOYBUTTONUP, SDL_JOYBUTTONDOWN, SDL_JOYHATMOTION, SDL_JOYBALLMOTION, SDL_JOYAXISMOTION,
    };
    for(const auto& et : enabledEvents)
      SDL_EventState(et, SDL_ENABLE);
    // SDL_AddEventWatch(&processEvent, NULL); //these don't work
    // SDL_SetEventFilter(&processEvent, NULL);
  };

  void pollInput() {//static
    SDL_Event event;
    concurrentReadLock_write lock(&allInputData_mutex);
    //reset input counters
    for(auto& pair : allInputData) {
      compositeInputData& cid = pair.second;
      cid.firstTime = cid.lastTime;//end of last frame polling
      for(int i = 0;i < 3;i++) {
	cid.axes[i].delta = cid.axes[i].numNegative = cid.axes[i].numPositive = cid.axes[i].numZero = 0;
	cid.axes[i].average = cid.axes[i].min = cid.axes[i].max = cid.axes[i].current;
      }
    }
    //process new events (since last frame stopped polling)
    while(SDL_PollEvent(&event)) {
      processEvent(NULL, &event);
    }
    //fill in the space between the most recent event and now with the current value
    uint32_t now = SDL_GetTicks();
    for(auto& pair : allInputData)
      handleEvent(now, pair.first);
  };

  void getInput(const inputIdentifier& ii, compositeInputData& out) {
    concurrentReadLock_read lock(&allInputData_mutex);
    out = allInputData[ii];
  };

  bool getButton(const inputIdentifier& ii) {
    compositeInputData cid;
    getInput(ii, cid);
    return cid.axes[0].isPressed();
  };

  void getLatest(inputPair& out) {
    concurrentReadLock_read lock(&allInputData_mutex);
    inputPair ret;
    for(const auto& pair : allInputData)
      if(pair.second.lastTime > ret.data.lastTime)
	ret = { pair.first, pair.second };
  };

  bool compositeInputData::axis::isPressed() {
    return current > 0.5f || numPositive > 0;
  };


  bool compositeInputData::axis::justChanged() {
    return min != max;
  };


  bool compositeInputData::axis::justPressed() {
    return isPressed() && delta > 0;
  };


  bool compositeInputData::axis::justReleased() {
    return !isPressed() && delta < 0;
  };


  bool compositeInputData::axis::justClicked() {
    return delta <= 0 && max > current;
  };

}
