#pragma once

#include <array>

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/water_heater/water_heater.h"

namespace esphome::tuya {

class TuyaWaterHeater final : public water_heater::WaterHeater, public Component {
 public:
  TuyaWaterHeater() { this->mode_values_.fill(MODE_VALUE_UNSET); }

  void setup() override;
  void dump_config() override;

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }

  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_target_temperature_id(uint8_t target_temperature_id) {
    this->target_temperature_id_ = target_temperature_id;
  }
  void set_current_temperature_id(uint8_t current_temperature_id) {
    this->current_temperature_id_ = current_temperature_id;
  }
  void set_target_temperature_multiplier(float multiplier) { this->target_temperature_multiplier_ = multiplier; }
  void set_current_temperature_multiplier(float multiplier) { this->current_temperature_multiplier_ = multiplier; }

  void set_mode_id(uint8_t mode_id) { this->mode_id_ = mode_id; }
  /// Map a WaterHeaterMode to the raw Tuya enum value the device uses for it.
  void set_mode_value(water_heater::WaterHeaterMode mode, uint8_t value) { this->mode_values_[mode] = value; }

  void set_supported_modes(const std::initializer_list<water_heater::WaterHeaterMode> &modes) {
    this->supported_modes_ = modes;
  }

  water_heater::WaterHeaterCallInternal make_call() override;

 protected:
  void control(const water_heater::WaterHeaterCall &call) override;
  water_heater::WaterHeaterTraits traits() override;

  /// Map a Tuya mode datapoint enum value to a WaterHeaterMode. Returns true when a mapping
  /// exists, writing the result to \p mode.
  bool mode_from_value_(uint8_t value, water_heater::WaterHeaterMode &mode) const;
  /// Map a WaterHeaterMode to its configured Tuya enum value. Returns true when a mapping exists.
  bool value_from_mode_(water_heater::WaterHeaterMode mode, uint8_t &value) const;

  Tuya *parent_{nullptr};
  optional<uint8_t> switch_id_{};
  optional<uint8_t> target_temperature_id_{};
  optional<uint8_t> current_temperature_id_{};
  optional<uint8_t> mode_id_{};
  float target_temperature_multiplier_{1.0f};
  float current_temperature_multiplier_{1.0f};
  water_heater::WaterHeaterModeMask supported_modes_;
  /// Raw Tuya enum value for each WaterHeaterMode, or MODE_VALUE_UNSET when not configured.
  /// Indexed by WaterHeaterMode (0..WATER_HEATER_MODE_GAS).
  static constexpr uint8_t MODE_VALUE_UNSET = 0xFF;
  std::array<uint8_t, water_heater::WATER_HEATER_MODE_GAS + 1> mode_values_{};
  /// Last non-OFF mode reported by the mode datapoint, applied when the heater turns on.
  optional<water_heater::WaterHeaterMode> last_reported_mode_{};
  bool is_on_{false};

  /// The mode to show when the heater is on but no mode datapoint value is known yet: the last
  /// reported mode, else the first configured supported non-OFF mode, else ELECTRIC.
  water_heater::WaterHeaterMode default_on_mode_() const;
};

}  // namespace esphome::tuya
