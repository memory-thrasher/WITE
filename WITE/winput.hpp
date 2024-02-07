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
