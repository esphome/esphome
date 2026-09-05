#pragma once

namespace esphome::sharp_ac {

static constexpr int ION_MODE = 0x80;

enum class PowerMode { HEAT = 0x1, COOL = 0x2, DRY = 0x3, FAN = 0x4 };

enum class SwingHorizontal {
  SWING = 0xF,
  MIDDLE = 0x1,
  RIGHT = 0x2,
  LEFT = 0x3,
};

enum class Preset {
  NONE = 0x0,
  ECO = 0x1,
  FULLPOWER = 0x2,
};

enum class FanMode { FAN_LOW = 0x4, FAN_MID = 0x3, FAN_HIGH = 0x5, FAN_HIGHEST = 0x7, FAN_AUTO = 0x2 };

enum class SwingVertical {
  SWING = 0xF,
  AUTO_POSITION = 0x8,
  HIGHEST = 0x9,
  HIGH_POS = 0xA,
  MID = 0xB,
  LOW_POS = 0xC,
  LOWEST = 0xD,
};

}  // namespace esphome::sharp_ac
