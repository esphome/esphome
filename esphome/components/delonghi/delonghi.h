#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace delonghi {

// Values for DELONGHI ARC43XXX IR Controllers
const uint8_t DELONGHI_ADDRESS = 83;

// Temperature
const uint8_t DELONGHI_TEMP_MIN = 13;          // Celsius
const uint8_t DELONGHI_TEMP_MAX = 32;          // Celsius
const uint8_t DELONGHI_TEMP_OFFSET_COOL = 17;  // Celsius
const uint8_t DELONGHI_TEMP_OFFSET_HEAT = 12;  // Celsius

// Modes
const uint8_t DELONGHI_MODE_AUTO = 0b1000;
const uint8_t DELONGHI_MODE_COOL = 0b0000;
const uint8_t DELONGHI_MODE_HEAT = 0b0110;
const uint8_t DELONGHI_MODE_DRY = 0b0010;
const uint8_t DELONGHI_MODE_FAN = 0b0100;
const uint8_t DELONGHI_MODE_OFF = 0b0000;
const uint8_t DELONGHI_MODE_ON = 0b0001;

// Fan Speed
const uint8_t DELONGHI_FAN_AUTO = 0b00;
const uint8_t DELONGHI_FAN_HIGH = 0b01;
const uint8_t DELONGHI_FAN_MEDIUM = 0b10;
const uint8_t DELONGHI_FAN_LOW = 0b11;

// IR Transmission - similar to NEC1
const uint32_t DELONGHI_IR_FREQUENCY = 38000;
const uint32_t DELONGHI_HEADER_MARK = 9000;
const uint32_t DELONGHI_HEADER_SPACE = 4500;
const uint32_t DELONGHI_BIT_MARK = 465;
const uint32_t DELONGHI_ONE_SPACE = 1750;
const uint32_t DELONGHI_ZERO_SPACE = 670;

// State Frame size
const uint8_t DELONGHI_STATE_FRAME_SIZE = 8;

class DelonghiClimate : public climate_ir::ClimateIR {
 public:
  DelonghiClimate()
      : climate_ir::ClimateIR(DELONGHI_TEMP_MIN, DELONGHI_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  uint8_t operation_mode_();
  uint16_t fan_speed_();
  uint8_t temperature_();
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(const uint8_t frame[]);
};

}  // namespace delonghi
}  // namespace esphome
