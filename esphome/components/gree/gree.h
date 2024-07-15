#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace gree {

// Values for GREE IR Controllers
// Temperature
const uint8_t GREE_TEMP_MIN = 16;  // Celsius
const uint8_t GREE_TEMP_MAX = 30;  // Celsius

// Modes
const uint8_t GREE_MODE_AUTO = 0x00;
const uint8_t GREE_MODE_COOL = 0x01;
const uint8_t GREE_MODE_HEAT = 0x04;
const uint8_t GREE_MODE_DRY = 0x02;
const uint8_t GREE_MODE_FAN = 0x03;

const uint8_t GREE_MODE_OFF = 0x00;
const uint8_t GREE_MODE_ON = 0x08;

// Fan Speed
const uint8_t GREE_FAN_AUTO = 0x00;
const uint8_t GREE_FAN_1 = 0x10;
const uint8_t GREE_FAN_2 = 0x20;
const uint8_t GREE_FAN_3 = 0x30;
const uint8_t GREE_FAN_TURBO = 0x80;

// IR Transmission
const uint32_t GREE_IR_FREQUENCY = 38000;
const uint32_t GREE_HEADER_MARK = 9000;
const uint32_t GREE_HEADER_SPACE = 4000;
const uint32_t GREE_BIT_MARK = 620;
const uint32_t GREE_ONE_SPACE = 1600;
const uint32_t GREE_ZERO_SPACE = 540;
const uint32_t GREE_MESSAGE_SPACE = 19000;

// Timing specific for YAC features (I-Feel mode)
const uint32_t GREE_YAC_HEADER_MARK = 6000;
const uint32_t GREE_YAC_HEADER_SPACE = 3000;
const uint32_t GREE_YAC_BIT_MARK = 650;

// Timing specific to YAC1FB9
const uint32_t GREE_YAC1FB9_HEADER_SPACE = 4500;
const uint32_t GREE_YAC1FB9_MESSAGE_SPACE = 19980;

// State Frame size
const uint8_t GREE_STATE_FRAME_SIZE = 8;

// Only available on YAN
// Vertical air directions. Note that these cannot be set on all heat pumps
const uint8_t GREE_VDIR_AUTO = 0x00;
const uint8_t GREE_VDIR_MANUAL = 0x00;
const uint8_t GREE_VDIR_SWING = 0x01;
const uint8_t GREE_VDIR_UP = 0x02;
const uint8_t GREE_VDIR_MUP = 0x03;
const uint8_t GREE_VDIR_MIDDLE = 0x04;
const uint8_t GREE_VDIR_MDOWN = 0x05;
const uint8_t GREE_VDIR_DOWN = 0x06;

// Only available on YAC
// Horizontal air directions. Note that these cannot be set on all heat pumps
const uint8_t GREE_HDIR_AUTO = 0x00;
const uint8_t GREE_HDIR_MANUAL = 0x00;
const uint8_t GREE_HDIR_SWING = 0x01;
const uint8_t GREE_HDIR_LEFT = 0x02;
const uint8_t GREE_HDIR_MLEFT = 0x03;
const uint8_t GREE_HDIR_MIDDLE = 0x04;
const uint8_t GREE_HDIR_MRIGHT = 0x05;
const uint8_t GREE_HDIR_RIGHT = 0x06;

// Model codes
enum Model { GREE_GENERIC, GREE_YAN, GREE_YAA, GREE_YAC, GREE_YAC1FB9 };

class GreeClimate : public climate_ir::ClimateIR {
 public:
  GreeClimate()
      : climate_ir::ClimateIR(GREE_TEMP_MIN, GREE_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

  void set_model(Model model);

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;

  uint8_t operation_mode_();
  uint8_t fan_speed_();
  uint8_t horizontal_swing_();
  uint8_t vertical_swing_();
  uint8_t temperature_();

  Model model_{};
};

}  // namespace gree
}  // namespace esphome
