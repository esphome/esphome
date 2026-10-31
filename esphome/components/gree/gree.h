#pragma once

#include <array>

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/core/defines.h"
#ifdef USE_GREE_FEATURE_SWITCH
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

// YAW1F shares the longer YAC1FB9 header and inter-block gap.
static constexpr uint32_t GREE_YAC1FB9_HEADER_SPACE = 4500;
static constexpr uint32_t GREE_YAC1FB9_MESSAGE_SPACE = 19980;
// Gap between the YAW1F primary command and its fixed second message.
static constexpr uint32_t GREE_YAW1F_FOLLOWUP_SPACE = 40000;

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
// YX1FF vane-position byte while swinging
static constexpr uint8_t GREE_YX1FF_VANE_SWING = 0x11;
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

// Feature bits in byte 2
static constexpr uint8_t GREE_MODE_BIT_TURBO = 0x10;
static constexpr uint8_t GREE_MODE_BIT_LIGHT = 0x20;
static constexpr uint8_t GREE_MODE_BIT_HEALTH = 0x40;
static constexpr uint8_t GREE_MODE_BIT_XFAN = 0x80;

// Byte 3 Fahrenheit bits
static constexpr uint8_t GREE_FAHRENHEIT_BIT = 0x08;
static constexpr uint8_t GREE_TEMP_HALF_DEGREE_BIT = 0x04;  // Half a degree Celsius added to the temperature

#ifdef USE_GREE_FAHRENHEIT
/// Return k = 2 * temp_field + half_degree_bit (0..28) for a Celsius target. The unit displays
/// trunc(celsius_to_fahrenheit(GREE_TEMP_MIN + k / 2)), so k is the smallest value that shows the target in F.
uint8_t fahrenheit_half_steps(float target_celsius);
#endif

// Model-specific meanings for the same bit positions on YB1FA/YX1FF.
static constexpr uint8_t GREE_FAN_TURBO_BIT = 0x10;
static constexpr uint8_t GREE_MODEL_A_BIT = 0x40;

// Only available on YX1FF
// Sleep preset mode
static constexpr uint8_t GREE_PRESET_NONE = 0x00;
static constexpr uint8_t GREE_PRESET_SLEEP = 0x01;
static constexpr uint8_t GREE_PRESET_SLEEP_BIT = 0x80;

// Model codes
enum Model : uint8_t {
  GREE_GENERIC,
  GREE_YAN,
  GREE_YAA,
  GREE_YAC,
  GREE_YAC1FB9,
  GREE_YB1FA,
  GREE_YX1FF,
  GREE_YAG,
  GREE_YAW1F
};

enum GreeFeature : uint8_t {
  GREE_FEATURE_TURBO = 0,
  GREE_FEATURE_LIGHT = 1,
  GREE_FEATURE_HEALTH = 2,
  GREE_FEATURE_XFAN = 3,
};

static constexpr uint8_t gree_feature_bit(GreeFeature feature) { return GREE_MODE_BIT_TURBO << feature; }

using GreeState = std::array<uint8_t, GREE_STATE_FRAME_SIZE>;

struct GreeClimateData {
  climate::ClimateMode mode;
  uint8_t target_temperature;
  climate::ClimateFanMode fan_mode;
  climate::ClimateSwingMode swing_mode;
  climate::ClimatePreset preset;
  uint8_t feature_bits{0};
  // The unit's panel shows Fahrenheit; half_degree adds half a degree Celsius to target_temperature
  bool fahrenheit{false};
  bool half_degree{false};
};

class GreeProtocol {
 public:
  explicit GreeProtocol(Model model) : model_(model) {}

  void encode(remote_base::RemoteTransmitData *data, const GreeState &state) const;
#ifdef USE_GREE_RECEIVER
  optional<GreeState> decode(remote_base::RemoteReceiveData data) const;
#endif

  static uint8_t calculate_checksum(const GreeState &state);
#ifdef USE_GREE_RECEIVER
  static bool valid_checksum(const GreeState &state);
#endif

 protected:
  Model model_;
};

class GreeClimateCodec {
 public:
  static GreeState encode(Model model, const GreeClimateData &data);
#ifdef USE_GREE_RECEIVER
  /// Decode a frame whose checksum the protocol layer already checked.
  static optional<GreeClimateData> decode(Model model, const GreeState &state);
#endif

 protected:
  static uint8_t encode_operation_mode(climate::ClimateMode mode);
  static uint8_t encode_fan_mode(Model model, climate::ClimateFanMode fan_mode);
  static uint8_t encode_horizontal_swing(climate::ClimateSwingMode swing_mode);
  static uint8_t encode_vertical_swing(climate::ClimateSwingMode swing_mode);
#ifdef USE_GREE_RECEIVER
  static optional<GreeClimateData> decode_legacy(Model model, const GreeState &state);
  static optional<GreeClimateData> decode_model_a(Model model, const GreeState &state);
#endif
};

class GreeClimate final : public climate_ir::ClimateIR {
 public:
  GreeClimate()
      : climate_ir::ClimateIR(GREE_TEMP_MIN, GREE_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

#ifdef USE_GREE_FEATURE_SWITCH
  void setup() override;
#endif
  void set_model(Model model);
#ifdef USE_GREE_FAHRENHEIT
  void set_use_fahrenheit(bool use_fahrenheit) { this->use_fahrenheit_ = use_fahrenheit; }
#endif
  void set_feature_state(GreeFeature feature, bool enabled);
  bool get_feature_state(GreeFeature feature) const;
#ifdef USE_GREE_FEATURE_SWITCH
  void register_feature_switch(GreeFeature feature, switch_::Switch *feature_switch);
#endif

 protected:
  void control(const climate::ClimateCall &call) override;
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
#ifdef USE_GREE_RECEIVER
  bool on_receive(remote_base::RemoteReceiveData data) override;
#endif

#ifdef USE_GREE_FEATURE_SWITCH
  void publish_feature_state_(GreeFeature feature);
#ifdef USE_GREE_RECEIVER
  void publish_feature_states_();
#endif

  std::array<switch_::Switch *, 4> feature_switches_{};
#endif

  Model model_{};
  uint8_t feature_bits_{0};
#ifdef USE_GREE_FAHRENHEIT
  bool use_fahrenheit_{false};
#endif
};

}  // namespace esphome::gree
