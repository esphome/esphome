#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TOSHIBA_RAS_25J2KVSG_ND

#include "esphome/core/preferences.h"
#include "toshiba.h"
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

namespace esphome::toshiba {

static constexpr float TOSHIBA_RAS_25J2KVSG_ND_FROST_TEMP_C_MIN = 5.0f;
static constexpr float TOSHIBA_RAS_25J2KVSG_ND_FROST_TEMP_C_MAX = 13.0f;

/// RAS-25J2KVSG-ND, driven like its WH-TA15PE remote
class ToshibaRas25j2kvsgNdClimate final : public ToshibaClimate {
 public:
  static constexpr uint32_t POWER_SELECTION_RESTORE_VERSION = 0x5371E24D;

  ToshibaRas25j2kvsgNdClimate() { this->set_model(MODEL_RAS_25J2KVSG_ND); }

  void setup() override;
  void step_vane();
  void set_power_selection(size_t index);
#ifdef USE_SELECT
  SUB_SELECT(power_selection)
#endif

 protected:
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
  // In the order of the select's options; 75% and 50% are also their byte 7 power bits.
  enum class ToshibaPowerSelection : uint8_t {
    TOSHIBA_POWER_SELECTION_100,
    TOSHIBA_POWER_SELECTION_75,
    TOSHIBA_POWER_SELECTION_50,
    TOSHIBA_POWER_SELECTION_SILENT_1,
    TOSHIBA_POWER_SELECTION_SILENT_2,
    TOSHIBA_POWER_SELECTION_COUNT,
  };

  static bool is_heating_preset(ToshibaPreset preset) {
    return preset >= ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1 &&
           preset <= ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  }
  static bool is_sleep_preset(ToshibaPreset preset) { return preset >= ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H; }
  static bool is_silent(ToshibaPowerSelection power) {
    return power >= ToshibaPowerSelection::TOSHIBA_POWER_SELECTION_SILENT_1;
  }
  static ToshibaPreset custom_preset_at(size_t index) {
    return static_cast<ToshibaPreset>(index + static_cast<uint8_t>(ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1));
  }
  static size_t custom_preset_index(ToshibaPreset preset) {
    return static_cast<uint8_t>(preset) - static_cast<uint8_t>(ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1);
  }
  static ToshibaPreset preset_from_climate(climate::ClimatePreset preset);
  static ToshibaPreset preset_from_custom(const char *custom_preset);
  static bool power_preset_supported(ToshibaPowerSelection power, ToshibaPreset preset);
  // Validates a 9 to 12 byte state frame and reads its preset and power selection.
  static bool parse_state(const uint8_t *message, uint8_t length, ToshibaPreset &preset, ToshibaPowerSelection &power);

  void control(const climate::ClimateCall &call) override;
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;
  // control() enforces the separate normal range.
  float temperature_min() override { return TOSHIBA_RAS_25J2KVSG_ND_FROST_TEMP_C_MIN; }
  void transmit_current_temperature() override {}
  void sanitize_restore_state(climate::ClimateDeviceRestoreState &restore) override;

  void transmit_frame_(ToshibaPreset preset);
  void transmit_motion_(uint8_t command, uint8_t repeat);
  void encode_motion_(remote_base::RemoteTransmitData *data, uint8_t command, uint8_t repeat);
  void perform_(remote_base::RemoteTransmitterBase::TransmitCall &transmit, bool swing);
  bool receive_frame_(const uint8_t *message, uint8_t length);
  bool receive_motion_(uint8_t command);
  ToshibaPreset toshiba_preset_() const;
  void apply_toshiba_preset_(ToshibaPreset preset);
  // Saves what leaving the preset restores; call before the target and fan change.
  void enter_preset_(ToshibaPreset previous, ToshibaPreset preset);
  uint8_t fan_code_() const;
  void apply_fan_code_(uint8_t code);
  // Returns true when the selection changed.
  bool store_power_selection_(ToshibaPowerSelection power);
  void publish_power_selection_();

  ESPPreferenceObject power_selection_pref_;
  // millis() until which a received SWING is a repeated copy or an echo of our own send
  uint32_t swing_ignore_until_{0};
  // The target to return to when frost protection ends
  uint8_t normal_target_temperature_{24};
  ToshibaPowerSelection power_selection_{ToshibaPowerSelection::TOSHIBA_POWER_SELECTION_100};
  uint8_t sleep_fan_{0};
};

}  // namespace esphome::toshiba

#endif  // USE_TOSHIBA_RAS_25J2KVSG_ND
