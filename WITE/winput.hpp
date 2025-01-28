/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/
#pragma once

namespace WITE::winput {

  enum class type_e : uint8_t { key, mouse, mouseButton, mouseWheel, joyButton, joyAxis };//controllerX = joyX

  struct inputIdentifier {
    type_e type;
    uint32_t controllerId;//which mouse, controller, joystick (keyboard is always 0)
    uint32_t controlId;//key, btn, axis etc;
    auto operator<=>(const inputIdentifier&) const = default;
  };

  struct compositeInputData {
    struct axis {
      uint16_t numNegative = 0, numPositive = 0, numZero = 0;
      uint32_t lastChange = 0;
      float average = 0, min = 0, max = 0, current = 0, delta = 0;
      bool isPressed();//regardless of when
      bool justChanged();//this is the first frame after a change
      bool justPressed();
      bool justReleased();
      bool justClicked();//press and release on the same frame, unlikely
    };
    uint32_t lastChange = 0;
    // uint32_t windowId; //if needed later, last event only, probably only useful for mouse location
    axis axes[3];
  };

  struct inputPair {
    inputIdentifier id;
    compositeInputData data;
  };

  constexpr size_t maxFrameKeyboardBuffer = 1024;
  extern uint32_t frameKeyboardBuffer[maxFrameKeyboardBuffer];

  constexpr inputIdentifier mouseWheel { type_e::mouseWheel, 0, 0 },
	      mouse { type_e::mouse, 0, 0 },
	      lmb { type_e::mouseButton, 0, 1 },
	      mmb { type_e::mouseButton, 0, 2 },
	      rmb { type_e::mouseButton, 0, 3 };

  template<uint32_t SDLK> struct key {
    static constexpr WITE::winput::inputIdentifier v { WITE::winput::type_e::key, 0, SDLK };
  };

  void initInput();
  void pollInput();
  void getInput(const inputIdentifier&, compositeInputData&);
  bool getButton(const inputIdentifier&);
  void getLatest(inputPair& out);//for creating control mappings
  uint32_t getFrameStart();

  template<uint32_t SDLK> bool keyPressed() {
    compositeInputData cid;
    getInput(key<SDLK>::v, cid);
    return cid.axes[0].isPressed();
  };

}
