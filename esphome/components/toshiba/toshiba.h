#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/remote_base/toshiba_ac_protocol.h"
#ifdef USE_TOSHIBA_RAS_25J2KVSG_ND
#include "esphome/core/preferences.h"
#endif
#if defined(USE_TOSHIBA_RAS_25J2KVSG_ND) && defined(USE_SELECT)
#include "esphome/components/select/select.h"
#endif

namespace esphome::toshiba {

static constexpr uint8_t TOSHIBA_HEADER_BYTE_0 = 0xF2;
static constexpr uint8_t TOSHIBA_HEADER_BYTE_1 = 0x0D;
static constexpr uint8_t TOSHIBA_COMMAND_DEFAULT = 0x01;
static constexpr uint8_t TOSHIBA_COMMAND_TIMER = 0x02;
static constexpr uint8_t TOSHIBA_COMMAND_POWER = 0x08;
static constexpr uint8_t TOSHIBA_FAN_SPEED_AUTO = 0x00;
static constexpr uint8_t TOSHIBA_FAN_SPEED_QUIET = 0x20;
static constexpr uint8_t TOSHIBA_FAN_SPEED_1 = 0x40;
static constexpr uint8_t TOSHIBA_FAN_SPEED_2 = 0x60;
static constexpr uint8_t TOSHIBA_FAN_SPEED_3 = 0x80;
static constexpr uint8_t TOSHIBA_FAN_SPEED_4 = 0xA0;
static constexpr uint8_t TOSHIBA_FAN_SPEED_5 = 0xC0;
static constexpr uint8_t TOSHIBA_POWER_HIGH = 0x01;
static constexpr uint8_t TOSHIBA_POWER_ECO = 0x03;
static constexpr uint8_t TOSHIBA_MOTION_SWING = 0x04;
static constexpr uint8_t TOSHIBA_MOTION_FIX = 0x00;

// Simple enum to represent models.
enum Model {
  MODEL_GENERIC = 0,           // Temperature range is from 17 to 30
  MODEL_RAC_PT1411HWRU_C = 1,  // Temperature range is from 16 to 30
  MODEL_RAC_PT1411HWRU_F = 2,  // Temperature range is from 16 to 30
  MODEL_RAS_2819T = 3,         // RAS-2819T protocol variant, temperature range 18 to 30
  MODEL_SEIYA = 4,             // Seiya family (RAS-B13E2KVG-E etc.), generic frame with a swing byte
  MODEL_RAS_B10N3KV2 = 5,      // RAS-B13 remote family, generic frame with IRremoteESP8266's TOSHIBA_AC timings
  MODEL_RAS_25J2KVSG_ND = 6,   // WH-TA15PE remote
};

// Supported temperature ranges
const float TOSHIBA_GENERIC_TEMP_C_MIN = 17.0;
const float TOSHIBA_GENERIC_TEMP_C_MAX = 30.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN = 16.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_C_MAX = 30.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_F_MIN = 60.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_F_MAX = 86.0;
const float TOSHIBA_RAS_2819T_TEMP_C_MIN = 18.0;
const float TOSHIBA_RAS_2819T_TEMP_C_MAX = 30.0;
static constexpr float TOSHIBA_RAS_25J2KVSG_ND_FROST_TEMP_C_MIN = 5.0f;
static constexpr float TOSHIBA_RAS_25J2KVSG_ND_FROST_TEMP_C_MAX = 13.0f;

class ToshibaClimate final : public climate_ir::ClimateIR {
 public:
  ToshibaClimate()
      : climate_ir::ClimateIR(TOSHIBA_GENERIC_TEMP_C_MIN, TOSHIBA_GENERIC_TEMP_C_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET}) {}

  void setup() override;
  void set_model(Model model) { this->model_ = model; }
  void fix();

#ifdef USE_TOSHIBA_RAS_25J2KVSG_ND
  void set_power_selection(size_t index);
#ifdef USE_SELECT
  SUB_SELECT(power_selection)
#endif
#endif

 protected:
#ifdef USE_TOSHIBA_RAS_25J2KVSG_ND
  enum class ToshibaPreset : uint8_t {
    TOSHIBA_PRESET_NONE,
    TOSHIBA_PRESET_BOOST,
    TOSHIBA_PRESET_ECO,
    TOSHIBA_PRESET_FIREPLACE_1,
    TOSHIBA_PRESET_FIREPLACE_2,
    TOSHIBA_PRESET_FROST_PROTECTION,
    TOSHIBA_PRESET_HADA_CARE,
    TOSHIBA_PRESET_SLEEP_1H,
    TOSHIBA_PRESET_SLEEP_3H,
    TOSHIBA_PRESET_SLEEP_5H,
    TOSHIBA_PRESET_SLEEP_9H,
  };
  static bool is_heating_preset(ToshibaPreset preset) {
    return preset >= ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1 &&
           preset <= ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  }
  static bool is_sleep_preset(ToshibaPreset preset) { return preset >= ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H; }
  static ToshibaPreset custom_preset_at(size_t index) {
    return static_cast<ToshibaPreset>(index + static_cast<uint8_t>(ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1));
  }
  static ToshibaPreset preset_from_custom(const char *custom_preset);
  // Validates a 9 to 12 byte state frame and reads its preset and power selection.
  static bool parse_ras_25j2kvsg_nd_state(const uint8_t *message, uint8_t length, ToshibaPreset &preset,
                                          uint8_t &power);
  void control(const climate::ClimateCall &call) override;
  void setup_ras_25j2kvsg_nd_();
  void restore_ras_25j2kvsg_nd_(climate::ClimateDeviceRestoreState &restore);
  void transmit_ras_25j2kvsg_nd_(ToshibaPreset preset);
  void transmit_ras_25j2kvsg_nd_motion_(uint8_t command, uint8_t repeat);
  void encode_ras_25j2kvsg_nd_motion_(remote_base::RemoteTransmitData *data, uint8_t command, uint8_t repeat);
  void perform_ras_25j2kvsg_nd_(remote_base::RemoteTransmitterBase::TransmitCall &transmit, bool swing);
  bool receive_ras_25j2kvsg_nd_(remote_base::RemoteReceiveData data);
  bool receive_ras_25j2kvsg_nd_frame_(const uint8_t *message, uint8_t length);
  bool receive_ras_25j2kvsg_nd_motion_(uint8_t command);
  ToshibaPreset ras_25j2kvsg_nd_preset_() const;
  void set_ras_25j2kvsg_nd_preset_(ToshibaPreset preset);
  uint8_t ras_25j2kvsg_nd_fan_() const;
  void set_ras_25j2kvsg_nd_fan_(uint8_t code);
  void publish_power_selection_();
  void set_ras_25j2kvsg_nd_power_(uint8_t power);
  bool validate_ras_25j2kvsg_nd_power_preset_(size_t power, ToshibaPreset preset) const;

  ESPPreferenceObject power_selection_pref_;
  uint32_t last_swing_received_{0};
  uint32_t last_swing_transmitted_{0};
  uint16_t swing_echo_duration_{0};
  uint8_t normal_target_temperature_{24};
  uint8_t power_selection_{0};
  uint8_t sleep_fan_{0};
#endif
  void transmit_state() override;
  void transmit_generic_();
  void transmit_rac_pt1411hwru_();
  void transmit_rac_pt1411hwru_temp_(bool cs_state = true, bool cs_send_update = true);
  void transmit_ras_2819t_();
  // Process RAS-2819T IR command data
  bool process_ras_2819t_command_(const remote_base::ToshibaAcData &toshiba_data);
  // Returns the header if valid, else returns zero
  uint8_t is_valid_rac_pt1411hwru_header_(const uint8_t *message);
  // Returns true if message is a valid RAC-PT1411HWRU IR message, regardless if first or second packet
  bool is_valid_rac_pt1411hwru_message_(const uint8_t *message);
  // Returns true if message1 and message 2 are the same
  bool compare_rac_pt1411hwru_packets_(const uint8_t *message1, const uint8_t *message2);
  bool on_receive(remote_base::RemoteReceiveData data) override;

 private:
  // Last swing mode sent or received, for models whose swing is a one-shot command
  climate::ClimateSwingMode last_swing_mode_{climate::CLIMATE_SWING_OFF};
  climate::ClimateMode last_mode_{climate::CLIMATE_MODE_OFF};
  optional<climate::ClimateFanMode> last_fan_mode_{};
  float last_target_temperature_{24.0f};

  float temperature_min_() {
#ifdef USE_TOSHIBA_RAS_25J2KVSG_ND
    if (this->model_ == MODEL_RAS_25J2KVSG_ND)
      return TOSHIBA_RAS_25J2KVSG_ND_FROST_TEMP_C_MIN;  // control() enforces the separate normal range.
#endif
    if (this->model_ == MODEL_RAC_PT1411HWRU_C || this->model_ == MODEL_RAC_PT1411HWRU_F)
      return TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN;
    if (this->model_ == MODEL_RAS_2819T)
      return TOSHIBA_RAS_2819T_TEMP_C_MIN;
    return TOSHIBA_GENERIC_TEMP_C_MIN;  // Default to GENERIC for unknown models
  }
  float temperature_max_() {
    if (this->model_ == MODEL_RAC_PT1411HWRU_C || this->model_ == MODEL_RAC_PT1411HWRU_F)
      return TOSHIBA_RAC_PT1411HWRU_TEMP_C_MAX;
    if (this->model_ == MODEL_RAS_2819T)
      return TOSHIBA_RAS_2819T_TEMP_C_MAX;
    return TOSHIBA_GENERIC_TEMP_C_MAX;  // Default to GENERIC for unknown models
  }
  climate::ClimateSwingModeMask toshiba_swing_modes_() {
    if (this->model_ == MODEL_GENERIC || this->model_ == MODEL_RAS_B10N3KV2)
      return climate::ClimateSwingModeMask();
    if (this->model_ == MODEL_SEIYA) {
      // No captured code stops the swing, so OFF is not offered; the remote only selects a direction
      return climate::ClimateSwingModeMask{climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL,
                                           climate::CLIMATE_SWING_BOTH};
    }
    return climate::ClimateSwingModeMask{climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL};
  }
  void encode_(remote_base::RemoteTransmitData *data, const uint8_t *message, uint8_t nbytes, uint8_t repeat);
  bool decode_(remote_base::RemoteReceiveData *data, uint8_t *message, uint8_t nbytes);
  // Writes the F2 0D header, the length byte and its inverse, and the trailing XOR of an F2 0D frame.
  static void finalize_frame(uint8_t *message, uint8_t length);
  static bool frame_checksum_valid(const uint8_t *message, uint8_t length) {
    return xor_bytes(message + 4, length - 5) == message[length - 1];
  }

  // Shared by every model that uses the F2 0D frame
  uint8_t encode_mode_fan_() const;
  void decode_mode_fan_temperature_(const uint8_t *message);
  uint8_t seiya_swing_code_() const;
  void seiya_decode_swing_(uint8_t code);

  Model model_;
};

}  // namespace esphome::toshiba
