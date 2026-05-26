#pragma once
#include "core_types.h"
#include "core_frame.h"

class SharpState {
 public:
  bool state;
  PowerMode mode;
  FanMode fan;
  SwingHorizontal swingH;
  SwingVertical swingV;
  int temperature;
  bool ion;
  Preset preset;

  SharpCommandFrame toFrame() {
    SharpCommandFrame frame;
    frame.setData(this);
    return frame;
  }

  SharpState()
      : state(false),
        mode(PowerMode::fan),
        fan(FanMode::low),
        swingH(SwingHorizontal::middle),
        swingV(SwingVertical::mid),
        temperature(25),
        ion(false),
        preset(Preset::NONE) {}

  SharpState(const SharpState &other) {
    state = other.state;
    mode = other.mode;
    fan = other.fan;
    temperature = other.temperature;
    swingH = other.swingH;
    swingV = other.swingV;
    ion = other.ion;
    preset = other.preset;
  }
};
