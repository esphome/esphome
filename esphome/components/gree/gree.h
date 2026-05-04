#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

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

// Mode bits (stored in byte 2 high nibble)
static constexpr uint8_t GREE_MODE_BIT_TURBO = 0x10;
static constexpr uint8_t GREE_MODE_BIT_LIGHT = 0x20;
static constexpr uint8_t GREE_MODE_BIT_HEALTH = 0x40;
static constexpr uint8_t GREE_MODE_BIT_XFAN = 0x80;

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
static constexpr uint32_t GREE_YAC1FB9_HEADER_SPACE = 4500;
static constexpr uint32_t GREE_YAC1FB9_MESSAGE_SPACE = 19980;

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
// Turbo (high) fan mode + sleep preset mode
static constexpr uint8_t GREE_FAN_TURBO = 0x80;
static constexpr uint8_t GREE_FAN_TURBO_BIT = 0x10;
static constexpr uint8_t GREE_PRESET_NONE = 0x00;
static constexpr uint8_t GREE_PRESET_SLEEP = 0x01;
static constexpr uint8_t GREE_PRESET_SLEEP_BIT = 0x80;

// Model codes
enum Model { GREE_GENERIC, GREE_YAN, GREE_YAA, GREE_YAC, GREE_YAC1FB9, GREE_YX1FF, GREE_YAG, GREE_YAP1F };

// Direction enums (used for default fixed vane positions)
enum HorizontalDirections {
  HORIZONTAL_DIRECTION_AUTO = GREE_HDIR_AUTO,
  HORIZONTAL_DIRECTION_LEFT = GREE_HDIR_LEFT,
  HORIZONTAL_DIRECTION_MLEFT = GREE_HDIR_MLEFT,
  HORIZONTAL_DIRECTION_MIDDLE = GREE_HDIR_MIDDLE,
  HORIZONTAL_DIRECTION_MRIGHT = GREE_HDIR_MRIGHT,
  HORIZONTAL_DIRECTION_RIGHT = GREE_HDIR_RIGHT,
};

enum VerticalDirections {
  VERTICAL_DIRECTION_AUTO = GREE_VDIR_AUTO,
  VERTICAL_DIRECTION_UP = GREE_VDIR_UP,
  VERTICAL_DIRECTION_MUP = GREE_VDIR_MUP,
  VERTICAL_DIRECTION_MIDDLE = GREE_VDIR_MIDDLE,
  VERTICAL_DIRECTION_MDOWN = GREE_VDIR_MDOWN,
  VERTICAL_DIRECTION_DOWN = GREE_VDIR_DOWN,
};

class GreeClimate : public climate_ir::ClimateIR {
 public:
  GreeClimate()
      : climate_ir::ClimateIR(GREE_TEMP_MIN, GREE_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

  void set_model(Model model);
  void set_mode_bit(uint8_t bit_mask, bool enabled);
  void set_horizontal_default(HorizontalDirections horizontal_direction) {
    this->default_horizontal_direction_ = horizontal_direction;
  }
  void set_vertical_default(VerticalDirections vertical_direction) {
    this->default_vertical_direction_ = vertical_direction;
  }

  void register_mode_bit_switch(uint8_t bit_mask, void *arg, void (*publish_state)(void *arg, bool state));

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  uint8_t operation_mode_();
  uint8_t fan_speed_();
  uint8_t horizontal_swing_();
  uint8_t vertical_swing_();
  uint8_t temperature_();
  uint8_t preset_();

  Model model_{};
  uint8_t mode_bits_{0};  // Combined mode bits for remote_state[2]

  HorizontalDirections default_horizontal_direction_{HORIZONTAL_DIRECTION_AUTO};
  VerticalDirections default_vertical_direction_{VERTICAL_DIRECTION_AUTO};

  uint8_t mode_bit_switch_masks_[4]{};
  void *mode_bit_switch_args_[4]{};
  void (*mode_bit_switch_publish_state_)(void *arg, bool state){nullptr};
  uint8_t mode_bit_switch_count_{0};
  void publish_mode_bit_switches_();
};

}  // namespace esphome::gree
