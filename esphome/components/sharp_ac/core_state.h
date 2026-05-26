#pragma once
#include "core_types.h"
#include "core_frame.h"

namespace esphome {
namespace sharp_ac {

class SharpState
{
public:
    bool state;
    PowerMode mode;
    FanMode fan;
    SwingHorizontal swingH;
    SwingVertical swingV;
    int temperature;
    bool ion;
    Preset preset;

    SharpCommandFrame to_frame()
    {
        SharpCommandFrame frame;
        frame.set_data(this);
        return frame;
    }

    SharpState()
        : state(false), mode(PowerMode::FAN), fan(FanMode::FAN_LOW), swingH(SwingHorizontal::MIDDLE),
          swingV(SwingVertical::MID), temperature(25), ion(false), preset(Preset::NONE)
    {
    }

    SharpState(const SharpState &other)
    {
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

}  // namespace sharp_ac
}  // namespace esphome