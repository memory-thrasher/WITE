/*
Copyright 2020-2024 Wafflecat Games, LLC

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
      uint16_t numNegative, numPositive, numZero;
      float average, min, max, current, delta;
      bool isPressed();//regardless of when
      bool justChanged();//this is the first frame after a change
      bool justPressed();
      bool justReleased();
      bool justClicked();//press and release on the same frame, unlikely
    };
    // syncLock mutex;
    uint32_t firstTime = 0, lastTime;
    // uint32_t windowId; //if needed later, last event only, probably only useful for mouse location
    axis axes[3];
  };

  constexpr inputIdentifier mouseWheel { type_e::mouseWheel, 0, 0 },
	      mouse { type_e::mouse, 0, 0 },
	      lmb { type_e::mouseButton, 0, 1 },
	      mmb { type_e::mouseButton, 0, 2 },
	      rmb { type_e::mouseButton, 0, 3 };

  void initInput();
  void pollInput();
  void getInput(const inputIdentifier&, compositeInputData&);
  bool getButton(const inputIdentifier&);

}
