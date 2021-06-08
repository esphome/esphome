#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/midea_dongle/midea_dongle.h"
#include "esphome/components/climate/climate.h"
#include "midea_frame.h"

namespace esphome {
namespace midea_ac {

class MideaAC : public midea_dongle::MideaAppliance, public climate::Climate, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void on_frame(const midea_dongle::Frame &frame) override;
  void on_update() override;
  void setup() override { this->parent_->set_appliance(this); }
  void set_midea_dongle_parent(midea_dongle::MideaDongle *parent) { this->parent_ = parent; }
  void set_outdoor_temperature_sensor(sensor::Sensor *sensor) { this->outdoor_sensor_ = sensor; }
  void set_humidity_setpoint_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_power_sensor(sensor::Sensor *sensor) { this->power_sensor_ = sensor; }
  void set_beeper_feedback(bool state) { this->beeper_feedback_ = state; }
  void set_swing_horizontal(bool state) { this->traits_swing_horizontal_ = state; }
  void set_swing_both(bool state) { this->traits_swing_both_ = state; }
  void set_preset_eco(bool state) { this->traits_preset_eco_ = state; }
  void set_preset_sleep(bool state) { this->traits_preset_sleep_ = state; }
  void set_preset_boost(bool state) { this->traits_preset_boost_ = state; }
  bool allow_preset(climate::ClimatePreset preset) const;
  void set_custom_fan_modes(std::vector<std::string> custom_fan_modes) {
    this->traits_custom_fan_modes_ = custom_fan_modes;
  }
  void set_custom_presets(std::vector<std::string> custom_presets) { this->traits_custom_presets_ = custom_presets; }
  bool allow_custom_preset(const std::string &custom_preset) const;

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  const QueryFrame query_frame_;
  const PowerQueryFrame power_frame_;
  CommandFrame cmd_frame_;
  midea_dongle::MideaDongle *parent_{nullptr};
  sensor::Sensor *outdoor_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  uint8_t request_num_{0};
  bool ctrl_request_{false};
  bool beeper_feedback_{false};
  bool traits_swing_horizontal_{false};
  bool traits_swing_both_{false};
  bool traits_preset_eco_{false};
  bool traits_preset_sleep_{false};
  bool traits_preset_boost_{false};
  std::vector<std::string> traits_custom_fan_modes_{{}};
  std::vector<std::string> traits_custom_presets_{{}};
};

}  // namespace midea_ac
}  // namespace esphome
