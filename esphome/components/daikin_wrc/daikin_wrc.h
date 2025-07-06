#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace daikin_wrc {

// Values for Daikin ARC43XXX IR Controllers
// Temperature
const uint8_t DAIKIN_WRC_TEMP_MIN = 16;  // Celsius
const uint8_t DAIKIN_WRC_TEMP_MAX = 30;  // Celsius

// Modes
const uint8_t DAIKIN_WRC_MODE_AUTO = 0xa;
const uint8_t DAIKIN_WRC_MODE_COOL = 0x2;
const uint8_t DAIKIN_WRC_MODE_HEAT = 0x8;
const uint8_t DAIKIN_WRC_MODE_DRY = 0x1;
const uint8_t DAIKIN_WRC_MODE_FAN = 0x4;
const uint8_t DAIKIN_WRC_MODE_OFF = 0x0;

// Fan Speed
const uint8_t DAIKIN_WRC_FAN_AUTO = 0x1;
const uint8_t DAIKIN_WRC_FAN_SILENT = 0x9;
const uint8_t DAIKIN_WRC_FAN_TURBO = 0x3;
const uint8_t DAIKIN_WRC_FAN_LOW = 0x8;
const uint8_t DAIKIN_WRC_FAN_MEDIUM = 0x4;
const uint8_t DAIKIN_WRC_FAN_HIGH = 0x2;

// IR Transmission
const uint32_t DAIKIN_WRC_IR_FREQUENCY = 38000;
const uint32_t DAIKIN_WRC_HEADER_MARK = 9800;
const uint32_t DAIKIN_WRC_HEADER_SPACE = 9600;
const uint32_t DAIKIN_WRC_HDR_MSG_MARK = 4650;
const uint32_t DAIKIN_WRC_HDR_MSG_SPACE = 2400;
const uint32_t DAIKIN_WRC_BIT_MARK = 400;
const uint32_t DAIKIN_WRC_ONE_SPACE = 900;
const uint32_t DAIKIN_WRC_ZERO_SPACE = 300;
const uint32_t DAIKIN_WRC_MESSAGE_SPACE = 20000;
const uint32_t DAIKIN_WRC_END_SPACE = 100000;

// State Frame size
const uint8_t DAIKIN_WRC_STATE_FRAME_SIZE = 16;

class DaikinWrcClimate : public climate_ir::ClimateIR {
 public:
  void set_state_sensor(binary_sensor::BinarySensor *bs) { this->state_sensor_ = bs; }

  DaikinWrcClimate()
      : climate_ir::ClimateIR(
            DAIKIN_WRC_TEMP_MIN, DAIKIN_WRC_TEMP_MAX, 1.0f, true, true,
            {climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
             climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
            {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
            {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_SLEEP, climate::CLIMATE_PRESET_BOOST}) {}

  void setup() override;

 protected:
  binary_sensor::BinarySensor *state_sensor_{nullptr};
  climate::ClimateMode previous_mode_ = climate::CLIMATE_MODE_OFF;

  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  void control(const climate::ClimateCall &call) override;
  uint8_t operation_mode_() const;
  uint8_t fan_speed_() const;
  uint8_t special_flags_() const;
  uint8_t temperature_() const;
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(const uint8_t frame[]);
};

}  // namespace daikin_wrc
}  // namespace esphome
