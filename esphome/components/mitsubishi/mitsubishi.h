#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

#include <cinttypes>

namespace esphome {
namespace mitsubishi {

// Temperature
const uint8_t MITSUBISHI_TEMP_MIN = 16;  // Celsius
const uint8_t MITSUBISHI_TEMP_MAX = 31;  // Celsius

// Fan mode
enum SetFanMode {
  MITSUBISHI_FAN_3L = 0,  // 3 levels + auto
  MITSUBISHI_FAN_4L,      // 4 levels + auto
  MITSUBISHI_FAN_Q4L,     // Quiet + 4 levels + auto
  //  MITSUBISHI_FAN_5L,      // 5 levels + auto
};

// Enum to represent horizontal directios
enum HorizontalDirection {
  HORIZONTAL_DIRECTION_LEFT = 0x10,
  HORIZONTAL_DIRECTION_MIDDLE_LEFT = 0x20,
  HORIZONTAL_DIRECTION_MIDDLE = 0x30,
  HORIZONTAL_DIRECTION_MIDDLE_RIGHT = 0x40,
  HORIZONTAL_DIRECTION_RIGHT = 0x50,
  HORIZONTAL_DIRECTION_SPLIT = 0x80,
};

// Enum to represent vertical directions
enum VerticalDirection {
  VERTICAL_DIRECTION_AUTO = 0x00,
  VERTICAL_DIRECTION_UP = 0x08,
  VERTICAL_DIRECTION_MIDDLE_UP = 0x10,
  VERTICAL_DIRECTION_MIDDLE = 0x18,
  VERTICAL_DIRECTION_MIDDLE_DOWN = 0x20,
  VERTICAL_DIRECTION_DOWN = 0x28,
};

class MitsubishiClimate : public climate_ir::ClimateIR {
 public:
  MitsubishiClimate()
      : climate_ir::ClimateIR(MITSUBISHI_TEMP_MIN, MITSUBISHI_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MIDDLE,
                               climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL},
                              {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_ECO, climate::CLIMATE_PRESET_BOOST,
                               climate::CLIMATE_PRESET_SLEEP}) {}

  void set_supports_cool(bool supports_cool) { this->supports_cool_ = supports_cool; }
  void set_supports_dry(bool supports_dry) { this->supports_dry_ = supports_dry; }
  void set_supports_fan_only(bool supports_fan_only) { this->supports_fan_only_ = supports_fan_only; }
  void set_supports_heat(bool supports_heat) { this->supports_heat_ = supports_heat; }

  void set_fan_mode(SetFanMode fan_mode) { this->fan_mode_ = fan_mode; }

  void set_horizontal_default(HorizontalDirection horizontal_direction) {
    this->default_horizontal_direction_ = horizontal_direction;
  }
  void set_vertical_default(VerticalDirection vertical_direction) {
    this->default_vertical_direction_ = vertical_direction;
  }

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(const uint8_t frame[]);

  SetFanMode fan_mode_;

  HorizontalDirection default_horizontal_direction_;
  VerticalDirection default_vertical_direction_;

  climate::ClimateTraits traits() override;
};

}  // namespace mitsubishi
}  // namespace esphome
