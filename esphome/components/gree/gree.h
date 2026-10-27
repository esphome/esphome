#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/core/defines.h"
#ifdef USE_GREE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::gree {

// Values for GREE IR Controllers
// Temperature
static constexpr uint8_t GREE_TEMP_MIN = 16;  // Celsius
static constexpr uint8_t GREE_TEMP_MAX = 30;  // Celsius

// Modes
static constexpr uint8_t GREE_MODE_AUTO = 0x00;
static constexpr uint8_t GREE_MODE_COOL = 0x01;
static constexpr uint8_t GREE_MODE_HEAT = 0x04;
static constexpr uint8_t GREE_MODE_DRY = 0x02;
static constexpr uint8_t GREE_MODE_FAN = 0x03;

static constexpr uint8_t GREE_MODE_OFF = 0x00;
static constexpr uint8_t GREE_MODE_ON = 0x08;

// Fan Speed
static constexpr uint8_t GREE_FAN_AUTO = 0x00;
static constexpr uint8_t GREE_FAN_1 = 0x10;
static constexpr uint8_t GREE_FAN_2 = 0x20;
static constexpr uint8_t GREE_FAN_3 = 0x30;

// IR Transmission
static constexpr uint32_t GREE_IR_FREQUENCY = 38000;
static constexpr uint32_t GREE_HEADER_MARK = 9000;
static constexpr uint32_t GREE_HEADER_SPACE = 4000;
static constexpr uint32_t GREE_BIT_MARK = 620;
static constexpr uint32_t GREE_ONE_SPACE = 1600;
static constexpr uint32_t GREE_ZERO_SPACE = 540;
static constexpr uint32_t GREE_MESSAGE_SPACE = 19000;

// Timing specific for YAC features (I-Feel mode)
static constexpr uint32_t GREE_YAC_HEADER_MARK = 6000;
static constexpr uint32_t GREE_YAC_HEADER_SPACE = 3000;
static constexpr uint32_t GREE_YAC_BIT_MARK = 650;

// Timing specific to YAC1FB9
// Also used by YAW1F
static constexpr uint32_t GREE_YAC1FB9_HEADER_SPACE = 4500;
static constexpr uint32_t GREE_YAC1FB9_MESSAGE_SPACE = 19980;

// YAW1F: every command is followed by a second 8-byte message after this gap
static constexpr uint32_t GREE_YAW1F_FOLLOWUP_SPACE = 40000;

// State Frame size
static constexpr uint8_t GREE_STATE_FRAME_SIZE = 8;

// Only available on YAN
// Vertical air directions. Note that these cannot be set on all heat pumps
static constexpr uint8_t GREE_VDIR_AUTO = 0x00;
static constexpr uint8_t GREE_VDIR_MANUAL = 0x00;
static constexpr uint8_t GREE_VDIR_SWING = 0x01;
static constexpr uint8_t GREE_VDIR_UP = 0x02;
static constexpr uint8_t GREE_VDIR_MUP = 0x03;
static constexpr uint8_t GREE_VDIR_MIDDLE = 0x04;
static constexpr uint8_t GREE_VDIR_MDOWN = 0x05;
static constexpr uint8_t GREE_VDIR_DOWN = 0x06;
// YX1FF vane-position byte while swinging
static constexpr uint8_t GREE_YX1FF_VANE_SWING = 0x11;

// Only available on YAC/YAG
// Horizontal air directions. Note that these cannot be set on all heat pumps
static constexpr uint8_t GREE_HDIR_AUTO = 0x00;
static constexpr uint8_t GREE_HDIR_MANUAL = 0x00;
static constexpr uint8_t GREE_HDIR_SWING = 0x01;
static constexpr uint8_t GREE_HDIR_LEFT = 0x02;
static constexpr uint8_t GREE_HDIR_MLEFT = 0x03;
static constexpr uint8_t GREE_HDIR_MIDDLE = 0x04;
static constexpr uint8_t GREE_HDIR_MRIGHT = 0x05;
static constexpr uint8_t GREE_HDIR_RIGHT = 0x06;

// Only available on YX1FF
// Turbo (high) fan mode
static constexpr uint8_t GREE_FAN_TURBO_BIT = 0x10;

// Available on YX1FF and YAC1FB9
// Sleep preset mode
static constexpr uint8_t GREE_PRESET_NONE = 0x00;
static constexpr uint8_t GREE_PRESET_SLEEP = 0x01;
static constexpr uint8_t GREE_PRESET_SLEEP_BIT = 0x80;

// Mode bits stored in remote_state[2] bits 4..7 for YAN/YAA/YAC/YAC1FB9.
static constexpr uint8_t GREE_MODE_BIT_TURBO = 0x10;
static constexpr uint8_t GREE_MODE_BIT_LIGHT = 0x20;
static constexpr uint8_t GREE_MODE_BIT_HEALTH = 0x40;
static constexpr uint8_t GREE_MODE_BIT_XFAN = 0x80;

// Byte 3 Fahrenheit bits
static constexpr uint8_t GREE_FAHRENHEIT_BIT = 0x08;
static constexpr uint8_t GREE_TEMP_HALF_DEGREE_BIT = 0x04;  // Half a degree Celsius added to the temperature

/// Checksum of a state frame: the low nibbles of bytes 0-3 plus the high nibbles of bytes 4-6 plus 0x0A, in the high
/// nibble.
inline uint8_t frame_checksum(const uint8_t *remote_state) {
  return (
      (((remote_state[0] & 0x0F) + (remote_state[1] & 0x0F) + (remote_state[2] & 0x0F) + (remote_state[3] & 0x0F) +
        ((remote_state[4] & 0xF0) >> 4) + ((remote_state[5] & 0xF0) >> 4) + ((remote_state[6] & 0xF0) >> 4) + 0x0A) &
       0x0F)
      << 4);
}

#ifdef USE_GREE_FAHRENHEIT
/// Return k = 2 * temp_field + half_degree_bit (0..28) for a Celsius target. The unit displays
/// trunc(celsius_to_fahrenheit(GREE_TEMP_MIN + k / 2)), so k is the smallest value that shows the target in F.
uint8_t fahrenheit_half_steps(float target_celsius);
#endif

// Model codes
enum Model : uint8_t { GREE_GENERIC, GREE_YAN, GREE_YAA, GREE_YAC, GREE_YAC1FB9, GREE_YX1FF, GREE_YAG, GREE_YAW1F };

class GreeClimate final : public climate_ir::ClimateIR {
 public:
  GreeClimate()
      : climate_ir::ClimateIR(GREE_TEMP_MIN, GREE_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

  void set_model(Model model);
#ifdef USE_GREE_FAHRENHEIT
  void set_use_fahrenheit(bool use_fahrenheit) { this->use_fahrenheit_ = use_fahrenheit; }
#endif
  void set_mode_bit(uint8_t bit_mask, bool enabled);
#ifdef USE_GREE_SWITCH
  void setup() override;
  SUB_SWITCH(turbo)
  SUB_SWITCH(light)
  SUB_SWITCH(health)
  SUB_SWITCH(xfan)
#endif

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;

  uint8_t operation_mode_();
  uint8_t fan_speed_();
  uint8_t horizontal_swing_();
  uint8_t vertical_swing_();
  uint8_t temperature_();
  uint8_t preset_();

  Model model_{};
  uint8_t mode_bits_{GREE_MODE_BIT_LIGHT};  // The display light is on until a switch says otherwise
#ifdef USE_GREE_FAHRENHEIT
  bool use_fahrenheit_{false};
#endif
};

}  // namespace esphome::gree
