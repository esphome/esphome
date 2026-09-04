#pragma once

#include <array>

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

// Vertical air directions. Note that these cannot be set on all heat pumps
static constexpr uint8_t GREE_VDIR_AUTO = 0x00;
static constexpr uint8_t GREE_VDIR_MANUAL = 0x00;
static constexpr uint8_t GREE_VDIR_SWING = 0x01;
static constexpr uint8_t GREE_VDIR_UP = 0x02;
static constexpr uint8_t GREE_VDIR_MUP = 0x03;
static constexpr uint8_t GREE_VDIR_MIDDLE = 0x04;
static constexpr uint8_t GREE_VDIR_MDOWN = 0x05;
static constexpr uint8_t GREE_VDIR_DOWN = 0x06;
static constexpr uint8_t GREE_VDIR_SWING_DOWN = 0x07;
static constexpr uint8_t GREE_VDIR_SWING_MIDDLE = 0x09;
static constexpr uint8_t GREE_VDIR_SWING_UP = 0x0B;

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

// Byte 2 feature bits
static constexpr uint8_t GREE_FAN_TURBO_BIT = 0x10;
static constexpr uint8_t GREE_LIGHT_BIT = 0x20;
static constexpr uint8_t GREE_MODEL_A_BIT = 0x40;
static constexpr uint8_t GREE_XFAN_BIT = 0x80;

// Only available on YX1FF
// Sleep preset mode
static constexpr uint8_t GREE_PRESET_NONE = 0x00;
static constexpr uint8_t GREE_PRESET_SLEEP = 0x01;
static constexpr uint8_t GREE_PRESET_SLEEP_BIT = 0x80;

// Model codes
enum Model { GREE_GENERIC, GREE_YAN, GREE_YAA, GREE_YAC, GREE_YAC1FB9, GREE_YB1FA, GREE_YX1FF, GREE_YAG };

using GreeState = std::array<uint8_t, GREE_STATE_FRAME_SIZE>;

struct GreeClimateData {
  climate::ClimateMode mode;
  uint8_t target_temperature;
  climate::ClimateFanMode fan_mode;
  climate::ClimateSwingMode swing_mode;
  climate::ClimatePreset preset;
};

class GreeProtocol {
 public:
  explicit GreeProtocol(Model model) : model_(model) {}

  void encode(remote_base::RemoteTransmitData *data, const GreeState &state) const;
  optional<GreeState> decode(remote_base::RemoteReceiveData data) const;

  static uint8_t calculate_checksum(const GreeState &state);
  static bool valid_checksum(const GreeState &state);

 protected:
  bool decode_bytes_(remote_base::RemoteReceiveData *data, GreeState *state, uint8_t offset) const;

  Model model_;
};

class GreeClimateCodec {
 public:
  static GreeState encode(Model model, const GreeClimateData &data, uint8_t mode_bits = 0);
  static optional<GreeClimateData> decode(Model model, const GreeState &state);

 protected:
  static uint8_t encode_operation_mode(climate::ClimateMode mode);
  static uint8_t encode_fan_mode(Model model, climate::ClimateFanMode fan_mode);
  static uint8_t encode_horizontal_swing(climate::ClimateSwingMode swing_mode);
  static uint8_t encode_vertical_swing(climate::ClimateSwingMode swing_mode);
  static optional<GreeClimateData> decode_model_a(Model model, const GreeState &state);
};

class GreeClimate final : public climate_ir::ClimateIR {
 public:
  GreeClimate()
      : climate_ir::ClimateIR(GREE_TEMP_MIN, GREE_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

  void set_model(Model model);
  void set_mode_bit(uint8_t bit_mask, bool enabled);

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;
  climate::ClimateTraits traits() override;

  Model model_{};
  uint8_t mode_bits_{0};  // Combined mode bits for remote_state[2]
};

}  // namespace esphome::gree
