#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/midea_dongle/midea_dongle.h"
#include "esphome/components/climate/climate.h"
#include "midea_frame.h"
#include "capabilities.h"

namespace esphome {
namespace midea_ac {

using sensor::Sensor;
using climate::ClimateCall;
using climate::ClimatePreset;
using climate::ClimateTraits;
using midea_dongle::Frame;
using midea_dongle::ResponseStatus;
using midea_dongle::ResponseHandler;
using midea_dongle::MideaDongle;

class MideaAC : public midea_dongle::MideaAppliance, public climate::Climate, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BEFORE_API; }
  void on_frame(const Frame &frame) override;
  void on_idle() override { this->get_status_(); }
  void setup() override;
  bool can_proceed() override { return this->capabilities_.is_ready(); }
  void set_dongle(MideaDongle *dongle) { this->dongle_ = dongle; }
  void set_outdoor_temperature_sensor(Sensor *sensor) { this->outdoor_sensor_ = sensor; }
  void set_humidity_setpoint_sensor(Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_power_sensor(Sensor *sensor) { this->power_sensor_ = sensor; }
  void set_beeper_feedback(bool state) { this->beeper_feedback_ = state; }
  void set_swing_horizontal(bool state) { this->traits_swing_horizontal_ = state; }
  void set_swing_both(bool state) { this->traits_swing_both_ = state; }
  void set_preset_sleep(bool state) { this->traits_preset_sleep_ = state; }
  bool allow_preset(ClimatePreset preset) const;
  void set_custom_fan_modes(std::set<std::string> custom_fan_modes) {
    this->traits_custom_fan_modes_ = std::move(custom_fan_modes);
  }
  void set_custom_presets(std::set<std::string> custom_presets) {
    this->traits_custom_presets_ = std::move(custom_presets);
  }
  bool allow_custom_preset(const std::string &custom_preset) const;
//  void set_preset_boost(bool state) { this->traits_preset_boost_ = state; }
//  void set_preset_eco(bool state) { this->traits_preset_eco_ = state; }

 protected:
  void control(const ClimateCall &call) override;
  ClimateTraits traits() override;
  ResponseStatus read_status_(const Frame &frame);
  void get_power_usage_();
  void get_capabilities_();
  void get_status_();

  MideaDongle *dongle_{nullptr};
  Sensor *outdoor_sensor_{nullptr};
  Sensor *humidity_sensor_{nullptr};
  Sensor *power_sensor_{nullptr};
  std::set<std::string> traits_custom_fan_modes_{{}};
  std::set<std::string> traits_custom_presets_{{}};
  CommandFrame cmd_frame_{};
  Capabilities capabilities_{};
  bool beeper_feedback_{false};
  bool traits_swing_horizontal_{false};
  bool traits_swing_both_{false};
  bool traits_preset_sleep_{false};
//  bool traits_preset_boost_{false};
//  bool traits_preset_eco_{false};
};

}  // namespace midea_ac
}  // namespace esphome
