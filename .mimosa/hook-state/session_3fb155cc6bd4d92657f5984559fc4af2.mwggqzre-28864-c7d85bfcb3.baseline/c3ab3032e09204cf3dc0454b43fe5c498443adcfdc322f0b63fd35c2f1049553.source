#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/water_heater/water_heater.h"

namespace esphome::tuya {

class TuyaWaterHeater final : public water_heater::WaterHeater, public Component {
 public:
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
  void set_eco_value(uint8_t value) { this->eco_value_ = value; }
  void set_electric_value(uint8_t value) { this->electric_value_ = value; }
  void set_performance_value(uint8_t value) { this->performance_value_ = value; }
  void set_high_demand_value(uint8_t value) { this->high_demand_value_ = value; }
  void set_heat_pump_value(uint8_t value) { this->heat_pump_value_ = value; }
  void set_gas_value(uint8_t value) { this->gas_value_ = value; }

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
  optional<uint8_t> eco_value_{};
  optional<uint8_t> electric_value_{};
  optional<uint8_t> performance_value_{};
  optional<uint8_t> high_demand_value_{};
  optional<uint8_t> heat_pump_value_{};
  optional<uint8_t> gas_value_{};
  float target_temperature_multiplier_{1.0f};
  float current_temperature_multiplier_{1.0f};
  water_heater::WaterHeaterModeMask supported_modes_;
  /// Last non-OFF mode reported by the mode datapoint, applied when the heater turns on.
  optional<water_heater::WaterHeaterMode> last_reported_mode_{};
  bool is_on_{false};

  /// The mode to show when the heater is on but no mode datapoint value is known yet: the last
  /// reported mode, else the first configured supported non-OFF mode, else ELECTRIC.
  water_heater::WaterHeaterMode default_on_mode_() const;
};

}  // namespace esphome::tuya
