#pragma once

namespace esphome {
namespace sharp_ac {

static constexpr int ION_MODE = 0x80;

enum class PowerMode
{
    HEAT = 0x1,
    COOL = 0x2,
    DRY = 0x3,
    FAN = 0x4
};

enum class SwingHorizontal {
  swing = 0xF,
  middle = 0x1,
  right = 0x2,
  left = 0x3,
};

enum class FanMode
{
    FAN_LOW = 0x4,
    FAN_MID = 0x3,
    FAN_HIGH = 0x5,
    FAN_HIGHEST = 0x7,
    FAN_AUTO = 0x2
};

enum class SwingVertical
{
    SWING = 0xF,
    AUTO_POSITION = 0x8,
    HIGHEST = 0x9,
    HIGH_POS = 0xA,
    MID = 0xB,
    LOW_POS = 0xC,
    LOWEST = 0xD,
};

enum class SwingHorizontal
{
    SWING = 0xF,
    MIDDLE = 0x1,
    RIGHT = 0x2,
    LEFT = 0x3,
};

}  // namespace sharp_ac
}  // namespace esphome
