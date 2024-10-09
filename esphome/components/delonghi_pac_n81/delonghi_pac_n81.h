#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace delonghi_pac_n81 {

// Values for DELONGHI ARC43XXX IR Controllers
const uint8_t DELONGHI_ADDRESS = 0x48;

// Temperature
const uint8_t DELONGHI_TEMP_MIN = 16;          // Celsius
const uint8_t DELONGHI_TEMP_MAX = 32;          // Celsius
const uint8_t DELONGHI_TEMP_OFFSET_COOL = 16;  // Celsius

// Modes
const uint8_t DELONGHI_MODE_COOL = 0b0001;
const uint8_t DELONGHI_MODE_DRY = 0b0100;
const uint8_t DELONGHI_MODE_FAN = 0b1000;
const uint8_t DELONGHI_MODE_DEFAULT = 0b0001;

// Fan Speed
const uint8_t DELONGHI_FAN_HIGH = 0b1000;
const uint8_t DELONGHI_FAN_MEDIUM = 0b0100;
const uint8_t DELONGHI_FAN_LOW = 0b0010;

// IR Transmission - similar to NEC1
const uint32_t DELONGHI_IR_FREQUENCY = 38000;
const uint32_t DELONGHI_HEADER_MARK = 9000;
const uint32_t DELONGHI_HEADER_SPACE = 4500;
const uint32_t DELONGHI_BIT_MARK = 560;
const uint32_t DELONGHI_ONE_SPACE = 1690;
const uint32_t DELONGHI_ZERO_SPACE = 560;

// State Frame size
const uint8_t DELONGHI_STATE_FRAME_SIZE = 4;

class DelonghiClimate : public climate_ir::ClimateIR {
 public:
  DelonghiClimate()
      : climate_ir::ClimateIR(DELONGHI_TEMP_MIN, DELONGHI_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH}) {}

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  uint8_t operation_mode_();
  uint16_t fan_speed_();
  uint8_t temperature_();
  uint8_t power_();
};

}  // namespace delonghi_pac_n81
}  // namespace esphome
