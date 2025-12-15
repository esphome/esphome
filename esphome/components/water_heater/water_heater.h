#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace water_heater {

enum WaterHeaterMode : uint32_t {
  WATER_HEATER_MODE_OFF = 0,
  WATER_HEATER_MODE_ECO = 1,
  WATER_HEATER_MODE_ELECTRIC = 2,
  WATER_HEATER_MODE_PERFORMANCE = 3,
  WATER_HEATER_MODE_HIGH_DEMAND = 4,
  WATER_HEATER_MODE_HEAT_PUMP = 5,
  WATER_HEATER_MODE_GAS = 6,
};

class WaterHeater;

class WaterHeaterCall {
 public:
  WaterHeaterCall(WaterHeater *parent);

  WaterHeaterCall &set_mode(WaterHeaterMode mode);
  WaterHeaterCall &set_mode(const std::string &mode);
  WaterHeaterCall &set_target_temperature(float temperature);

  void perform();

  const optional<WaterHeaterMode> &get_mode() const;
  const optional<float> &get_target_temperature() const;

 protected:
  void validate_();

  WaterHeater *parent_;
  optional<WaterHeaterMode> mode_;
  optional<float> target_temperature_;
};

class WaterHeaterTraits {
 public:
  void set_supports_current_temperature(bool supports_current_temperature);
  bool get_supports_current_temperature() const;

  void set_min_temperature(float min_temperature);
  float get_min_temperature() const;

  void set_max_temperature(float max_temperature);
  float get_max_temperature() const;

  void set_supported_modes(std::set<WaterHeaterMode> modes);
  const std::set<WaterHeaterMode> &get_supported_modes() const;
  bool supports_mode(WaterHeaterMode mode) const;

 protected:
  bool supports_current_temperature_{false};
  float min_temperature_{0.0f};
  float max_temperature_{0.0f};
  std::set<WaterHeaterMode> supported_modes_;
};

class WaterHeater : public EntityBase, public Component {
 public:
  WaterHeaterMode mode{WATER_HEATER_MODE_OFF};
  float current_temperature{NAN};
  float target_temperature{0.0f};

  void publish_state();

  WaterHeaterTraits get_traits();

  WaterHeaterCall make_call();

  virtual void control(const WaterHeaterCall &call) = 0;

  void set_visual_min_temperature_override(float min_temperature_override);
  void set_visual_max_temperature_override(float max_temperature_override);

  void setup() override;

 protected:
  virtual WaterHeaterTraits traits() = 0;

  optional<float> visual_min_temperature_override_{};
  optional<float> visual_max_temperature_override_{};

  ESPPreferenceObject pref_;
};

}  // namespace water_heater
}  // namespace esphome