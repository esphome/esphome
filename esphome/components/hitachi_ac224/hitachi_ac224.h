#pragma once

#include "esphome/core/log.h"
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace hitachi_ac224 {

const uint16_t HITACHI_AC224_HDR_MARK = 3300;
const uint16_t HITACHI_AC224_HDR_SPACE = 1700;
const uint16_t HITACHI_AC224_BIT_MARK = 400;
const uint16_t HITACHI_AC224_ONE_SPACE = 1250;
const uint16_t HITACHI_AC224_ZERO_SPACE = 500;
const uint32_t HITACHI_AC224_MIN_GAP = 100000;  // Just a guess.
const uint16_t HITACHI_AC224_FREQ = 38000;      // Hz.

const uint8_t HITACHI_AC224_TEMP_BYTE = 11;
const uint8_t HITACHI_AC224_TEMP_MIN = 16;  // 16C
const uint8_t HITACHI_AC224_TEMP_MAX = 32;  // 32C
const uint8_t HITACHI_AC224_TEMP_FAN = 64;  // 64C
const uint8_t HITACHI_AC224_TEMP_BOOST_BYTE = 9;
const uint8_t HITACHI_AC224_TEMP_BOOST_ON = 0x90;
const uint8_t HITACHI_AC224_TEMP_BOOST_OFF = 0x10;

const uint8_t HITACHI_AC224_MODE_BYTE = 10;
const uint8_t HITACHI_AC224_MODE_AUTO = 2;
const uint8_t HITACHI_AC224_MODE_HEAT = 3;
const uint8_t HITACHI_AC224_MODE_COOL = 4;
const uint8_t HITACHI_AC224_MODE_DRY = 5;
const uint8_t HITACHI_AC224_MODE_FAN = 0xC;

const uint8_t HITACHI_AC224_FAN_BYTE = 13;
const uint8_t HITACHI_AC224_FAN_AUTO = 1;
const uint8_t HITACHI_AC224_FAN_QUIET = 2;
const uint8_t HITACHI_AC224_FAN_LOW = 3;
const uint8_t HITACHI_AC224_FAN_MEDIUM = 4;
const uint8_t HITACHI_AC224_FAN_HIGH = 5;

const uint8_t HITACHI_AC224_POWER_BYTE = 17;
const uint8_t HITACHI_AC224_POWER_OFFSET = 0;  // Mask 0b0000000x

const uint8_t HITACHI_AC224_SWINGV_BYTE = 14;
const uint8_t HITACHI_AC224_SWINGV_OFFSET = 7;  // Mask 0bx0000000

const uint8_t HITACHI_AC224_SWINGH_BYTE = 15;
const uint8_t HITACHI_AC224_SWINGH_OFFSET = 7;  // Mask 0bx0000000

const uint8_t HITACHI_AC224_CHECKSUM_BYTE = 27;

const uint16_t HITACHI_AC224_STATE_LENGTH = 28;

class HitachiClimate : public climate_ir::ClimateIR {
 public:
  HitachiClimate()
      : climate_ir::ClimateIR(HITACHI_AC224_TEMP_MIN, HITACHI_AC224_TEMP_MAX, 1.0F, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_LOW,
                               climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_HORIZONTAL,
                               climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_BOTH}) {}

 protected:
  uint8_t remote_state_[HITACHI_AC224_STATE_LENGTH]{0x80, 0x08, 0x0C, 0x02, 0xFD, 0x80, 0x7F, 0x88, 0x48, 0x10,
                                                    0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x00, 0x00, 0x00,
                                                    0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00};
  uint8_t previous_temp_{27};
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  void set_checksum_();
  bool get_power_();
  void set_power_(bool on);
  uint8_t get_mode_();
  void set_mode_(uint8_t mode);
  void set_temp_(uint8_t celsius);
  uint8_t get_fan_();
  void set_fan_(uint8_t speed);
  void set_swing_v_(bool on);
  bool get_swing_v_();
  void set_swing_h_(bool on);
  bool get_swing_h_();
  void dump_state_(const char action[], uint8_t remote_state[]);
};

}  // namespace hitachi_ac224
}  // namespace esphome
