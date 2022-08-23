#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace daikin_brc {

// Values for Daikin BRC4CXXX IR Controllers
// Temperature
const uint8_t DAIKIN_BRC_TEMP_MIN_F = 60;                                // fahrenheit
const uint8_t DAIKIN_BRC_TEMP_MAX_F = 90;                                // fahrenheit
const float DAIKIN_BRC_TEMP_MIN_C = (DAIKIN_BRC_TEMP_MIN_F - 32) / 1.8;  // fahrenheit
const float DAIKIN_BRC_TEMP_MAX_C = (DAIKIN_BRC_TEMP_MAX_F - 32) / 1.8;  // fahrenheit

// Modes
const uint8_t DAIKIN_BRC_MODE_AUTO = 0x30;
const uint8_t DAIKIN_BRC_MODE_COOL = 0x20;
const uint8_t DAIKIN_BRC_MODE_HEAT = 0x10;
const uint8_t DAIKIN_BRC_MODE_DRY = 0x70;
const uint8_t DAIKIN_BRC_MODE_FAN = 0x00;
const uint8_t DAIKIN_BRC_MODE_OFF = 0x00;
const uint8_t DAIKIN_BRC_MODE_ON = 0x01;

// Fan Speed
const uint8_t DAIKIN_BRC_FAN_1 = 0x10;
const uint8_t DAIKIN_BRC_FAN_2 = 0x30;
const uint8_t DAIKIN_BRC_FAN_3 = 0x50;
const uint8_t DAIKIN_BRC_FAN_AUTO = 0xA0;

// IR Transmission
const uint32_t DAIKIN_BRC_IR_FREQUENCY = 38000;
const uint32_t DAIKIN_BRC_HEADER_MARK = 5070;
const uint32_t DAIKIN_BRC_HEADER_SPACE = 2140;
const uint32_t DAIKIN_BRC_BIT_MARK = 370;
const uint32_t DAIKIN_BRC_ONE_SPACE = 1780;
const uint32_t DAIKIN_BRC_ZERO_SPACE = 710;
const uint32_t DAIKIN_BRC_MESSAGE_SPACE = 29410;

const uint8_t DAIKIN_BRC_IR_DRY_FAN_TEMP_F = 72;            // Dry/Fan mode is always 17 Celsius.
const uint8_t DAIKIN_BRC_IR_DRY_FAN_TEMP_C = (17 - 9) * 2;  // Dry/Fan mode is always 17 Celsius.
const uint8_t DAIKIN_BRC_IR_SWING_ON = 0x5;
const uint8_t DAIKIN_BRC_IR_SWING_OFF = 0x6;
const uint8_t DAIKIN_BRC_IR_MODE_BUTTON = 0x4;  // This is set after a mode action

// State Frame size
const uint8_t DAIKIN_BRC_STATE_FRAME_SIZE = 15;
// Preamble size
const uint8_t DAIKIN_BRC_PREAMBLE_SIZE = 7;
// Transmit Frame size - includes a preamble
const uint8_t DAIKIN_BRC_TRANSMIT_FRAME_SIZE = DAIKIN_BRC_PREAMBLE_SIZE + DAIKIN_BRC_STATE_FRAME_SIZE;

class DaikinBrcClimate : public climate_ir::ClimateIR {
 public:
  DaikinBrcClimate()
      : climate_ir::ClimateIR(DAIKIN_BRC_TEMP_MIN_C, DAIKIN_BRC_TEMP_MAX_C, 0.5f, true, true,
                              {climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH}) {}

  /// Set use of Fahrenheit units
  void set_fahrenheit(bool value) {
    this->fahrenheit_ = value;
    this->temperature_step_ = value ? 0.5f : 1.0f;
  }

 protected:
  uint8_t mode_button_ = 0x00;
  // Capture if the MODE was changed
  void control(const climate::ClimateCall &call) override;
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  uint8_t alt_mode_();
  uint8_t operation_mode_();
  uint8_t fan_speed_swing_();
  uint8_t temperature_();
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(const uint8_t frame[]);
  bool fahrenheit_{false};
};

}  // namespace daikin_brc
}  // namespace esphome
