#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace daikin_brc52a61 {

// Values for Daikin BRC52A61 IR Controllers
// Temperature
const uint8_t DAIKIN_TEMP_MIN = 16;  // Celsius
const uint8_t DAIKIN_TEMP_MAX = 30;  // Celsius

// Modes
const uint8_t DAIKIN_MODE_AUTO = 0x00;
const uint8_t DAIKIN_MODE_DRY = 0x01;
const uint8_t DAIKIN_MODE_COOL = 0x02;
const uint8_t DAIKIN_MODE_FAN = 0x04;
const uint8_t DAIKIN_MODE_HEAT = 0x08;

// Fan Speed
const uint8_t DAIKIN_FAN_AUTO = 0x01;
const uint8_t DAIKIN_FAN_TURBO = 0x03;
const uint8_t DAIKIN_FAN_HIGH = 0x02;
const uint8_t DAIKIN_FAN_MED = 0x04;
const uint8_t DAIKIN_FAN_LOW = 0x08;
const uint8_t DAIKIN_FAN_QUIET = 0x09;

// IR Transmission
const uint32_t DAIKIN_IR_FREQUENCY = 38000;
const uint32_t DAIKIN_PRE_MARK = 9800;
const uint32_t DAIKIN_PRE_SPACE = 9800;

const uint32_t DAIKIN_HEADER_MARK = 4600;
const uint32_t DAIKIN_HEADER_SPACE = 2500;
const uint32_t DAIKIN_BIT_MARK = 400;
const uint32_t DAIKIN_ONE_SPACE = 920;
const uint32_t DAIKIN_ZERO_SPACE = 340;
const uint32_t DAIKIN_FOOTER_MARK = 4600;
const uint32_t DAIKIN_FOOTER_SPACE = 20000;

class DaikinClimate : public climate_ir::ClimateIR {
 public:
  DaikinClimate()
      : climate_ir::ClimateIR(DAIKIN_TEMP_MIN, DAIKIN_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_LOW,
                               climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  uint8_t operation_mode_();
  uint8_t fan_speed_();
  uint8_t temperature_();
  uint8_t swing_();

  // Handle received IR Buffer
  bool parse_state_frame_(struct IRData &irdata);
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace daikin_brc52a61
}  // namespace esphome
