#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace daikin_arc {

// Values for Daikin ARC43XXX IR Controllers
// Temperature
const uint8_t DAIKIN_TEMP_MIN = 10;  // Celsius
const uint8_t DAIKIN_TEMP_MAX = 30;  // Celsius

// Modes
const uint8_t DAIKIN_MODE_AUTO = 0x00;
const uint8_t DAIKIN_MODE_COOL = 0x30;
const uint8_t DAIKIN_MODE_HEAT = 0x40;
const uint8_t DAIKIN_MODE_DRY = 0x20;
const uint8_t DAIKIN_MODE_FAN = 0x60;
const uint8_t DAIKIN_MODE_OFF = 0x00;
const uint8_t DAIKIN_MODE_ON = 0x01;

// Fan Speed
const uint8_t DAIKIN_FAN_AUTO = 0xA0;
const uint8_t DAIKIN_FAN_SILENT = 0xB0;
const uint8_t DAIKIN_FAN_1 = 0x30;
const uint8_t DAIKIN_FAN_2 = 0x40;
const uint8_t DAIKIN_FAN_3 = 0x50;
const uint8_t DAIKIN_FAN_4 = 0x60;
const uint8_t DAIKIN_FAN_5 = 0x70;

// IR Transmission
const uint32_t DAIKIN_IR_FREQUENCY = 38000;
const uint32_t DAIKIN_ARC_PRE_MARK = 9950;
const uint32_t DAIKIN_ARC_PRE_SPACE = 25100;
const uint32_t DAIKIN_HEADER_MARK = 3450;
const uint32_t DAIKIN_HEADER_SPACE = 1760;
const uint32_t DAIKIN_BIT_MARK = 400;
const uint32_t DAIKIN_ONE_SPACE = 1300;
const uint32_t DAIKIN_ZERO_SPACE = 480;
const uint32_t DAIKIN_MESSAGE_SPACE = 35000;

const uint8_t DAIKIN_DBG_TOLERANCE = 25;
#define DAIKIN_DBG_LOWER(x) ((100 - DAIKIN_DBG_TOLERANCE) * (x) / 100U)
#define DAIKIN_DBG_UPPER(x) ((100 + DAIKIN_DBG_TOLERANCE) * (x) / 100U)

// State Frame size
const uint8_t DAIKIN_STATE_FRAME_SIZE = 19;

class DaikinArcClimate : public climate_ir::ClimateIR {
 public:
  DaikinArcClimate()
      : climate_ir::ClimateIR(DAIKIN_TEMP_MIN, DAIKIN_TEMP_MAX, 0.5f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

  void setup() override;

 protected:
  void control(const climate::ClimateCall &call) override;
  // Transmit via IR the state of this climate controller.
  void transmit_query_();
  void transmit_state() override;
  climate::ClimateTraits traits() override;
  uint8_t operation_mode_();
  uint16_t fan_speed_();
  uint8_t temperature_();
  uint8_t humidity_();
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(const uint8_t frame[]);
};

}  // namespace daikin_arc
}  // namespace esphome
