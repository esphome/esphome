#pragma once

#include "esphome/core/log.h"
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace hitachi_ac424 {

const uint16_t HITACHI_AC424_HDR_MARK = 3416;   // ac
const uint16_t HITACHI_AC424_HDR_SPACE = 1604;  // ac
const uint16_t HITACHI_AC424_BIT_MARK = 463;
const uint16_t HITACHI_AC424_ONE_SPACE = 1208;
const uint16_t HITACHI_AC424_ZERO_SPACE = 372;
const uint32_t HITACHI_AC424_MIN_GAP = 100000;  // just a guess.
const uint16_t HITACHI_AC424_FREQ = 38000;      // Hz.

const uint8_t HITACHI_AC424_BUTTON_BYTE = 11;
const uint8_t HITACHI_AC424_BUTTON_POWER = 0x13;
const uint8_t HITACHI_AC424_BUTTON_SLEEP = 0x31;
const uint8_t HITACHI_AC424_BUTTON_MODE = 0x41;
const uint8_t HITACHI_AC424_BUTTON_FAN = 0x42;
const uint8_t HITACHI_AC424_BUTTON_TEMP_DOWN = 0x43;
const uint8_t HITACHI_AC424_BUTTON_TEMP_UP = 0x44;
const uint8_t HITACHI_AC424_BUTTON_SWINGV = 0x81;
const uint8_t HITACHI_AC424_BUTTON_SWINGH = 0x8C;
const uint8_t HITACHI_AC424_BUTTON_MILDEWPROOF = 0xE2;

const uint8_t HITACHI_AC424_TEMP_BYTE = 13;
const uint8_t HITACHI_AC424_TEMP_OFFSET = 2;
const uint8_t HITACHI_AC424_TEMP_SIZE = 6;
const uint8_t HITACHI_AC424_TEMP_MIN = 16;  // 16C
const uint8_t HITACHI_AC424_TEMP_MAX = 32;  // 32C
const uint8_t HITACHI_AC424_TEMP_FAN = 27;  // 27C

const uint8_t HITACHI_AC424_TIMER_BYTE = 15;

const uint8_t HITACHI_AC424_MODE_BYTE = 25;
const uint8_t HITACHI_AC424_MODE_FAN = 1;
const uint8_t HITACHI_AC424_MODE_COOL = 3;
const uint8_t HITACHI_AC424_MODE_DRY = 5;
const uint8_t HITACHI_AC424_MODE_HEAT = 6;
const uint8_t HITACHI_AC424_MODE_AUTO = 14;
const uint8_t HITACHI_AC424_MODE_POWERFUL = 19;

const uint8_t HITACHI_AC424_FAN_BYTE = HITACHI_AC424_MODE_BYTE;
const uint8_t HITACHI_AC424_FAN_MIN = 1;
const uint8_t HITACHI_AC424_FAN_LOW = 2;
const uint8_t HITACHI_AC424_FAN_MEDIUM = 3;
const uint8_t HITACHI_AC424_FAN_HIGH = 4;
const uint8_t HITACHI_AC424_FAN_AUTO = 5;
const uint8_t HITACHI_AC424_FAN_MAX = 6;
const uint8_t HITACHI_AC424_FAN_MAX_DRY = 2;

const uint8_t HITACHI_AC424_POWER_BYTE = 27;
const uint8_t HITACHI_AC424_POWER_ON = 0xF1;
const uint8_t HITACHI_AC424_POWER_OFF = 0xE1;

const uint8_t HITACHI_AC424_SWINGH_BYTE = 35;
const uint8_t HITACHI_AC424_SWINGH_OFFSET = 0;     // Mask 0b00000xxx
const uint8_t HITACHI_AC424_SWINGH_SIZE = 3;       // Mask 0b00000xxx
const uint8_t HITACHI_AC424_SWINGH_AUTO = 0;       // 0b000
const uint8_t HITACHI_AC424_SWINGH_RIGHT_MAX = 1;  // 0b001
const uint8_t HITACHI_AC424_SWINGH_RIGHT = 2;      // 0b010
const uint8_t HITACHI_AC424_SWINGH_MIDDLE = 3;     // 0b011
const uint8_t HITACHI_AC424_SWINGH_LEFT = 4;       // 0b100
const uint8_t HITACHI_AC424_SWINGH_LEFT_MAX = 5;   // 0b101

const uint8_t HITACHI_AC424_SWINGV_BYTE = 37;
const uint8_t HITACHI_AC424_SWINGV_OFFSET = 5;  // Mask 0b00x00000

const uint8_t HITACHI_AC424_MILDEWPROOF_BYTE = HITACHI_AC424_SWINGV_BYTE;
const uint8_t HITACHI_AC424_MILDEWPROOF_OFFSET = 2;  // Mask 0b00000x00

const uint16_t HITACHI_AC424_STATE_LENGTH = 53;
const uint16_t HITACHI_AC424_BITS = HITACHI_AC424_STATE_LENGTH * 8;

#define HITACHI_AC424_GETBIT8(a, b) ((a) & ((uint8_t) 1 << (b)))
#define HITACHI_AC424_GETBITS8(data, offset, size) \
  (((data) & (((uint8_t) UINT8_MAX >> (8 - (size))) << (offset))) >> (offset))

class HitachiClimate : public climate_ir::ClimateIR {
 public:
  HitachiClimate()
      : climate_ir::ClimateIR(HITACHI_AC424_TEMP_MIN, HITACHI_AC424_TEMP_MAX, 1.0F, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_HORIZONTAL}) {}

 protected:
  uint8_t remote_state_[HITACHI_AC424_STATE_LENGTH]{
      0x01, 0x10, 0x00, 0x40, 0xBF, 0xFF, 0x00, 0xCC, 0x33, 0x92, 0x6D, 0x13, 0xEC, 0x5C, 0xA3, 0x00, 0xFF, 0x00,
      0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x53, 0xAC, 0xF1, 0x0E, 0x00, 0xFF, 0x00, 0xFF, 0x80, 0x7F, 0x03,
      0xFC, 0x01, 0xFE, 0x88, 0x77, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
  uint8_t previous_temp_{27};
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  bool get_power_();
  void set_power_(bool on);
  uint8_t get_mode_();
  void set_mode_(uint8_t mode);
  void set_temp_(uint8_t celsius, bool set_previous = false);
  uint8_t get_fan_();
  void set_fan_(uint8_t speed);
  void set_swing_v_toggle_(bool on);
  bool get_swing_v_toggle_();
  void set_swing_v_(bool on);
  bool get_swing_v_();
  void set_swing_h_(uint8_t position);
  uint8_t get_swing_h_();
  uint8_t get_button_();
  void set_button_(uint8_t button);
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_mode_(const uint8_t remote_state[]);
  bool parse_temperature_(const uint8_t remote_state[]);
  bool parse_fan_(const uint8_t remote_state[]);
  bool parse_swing_(const uint8_t remote_state[]);
  bool parse_state_frame_(const uint8_t frame[]);
  void dump_state_(const char action[], uint8_t remote_state[]);
};

}  // namespace hitachi_ac424
}  // namespace esphome
