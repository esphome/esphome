#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/midea_dongle/midea_dongle.h"
#include "esphome/components/climate/climate.h"
#include "midea_frame.h"
#include "midea_ir.h"
#include "capabilities.h"

namespace esphome {
namespace midea_ac {

using sensor::Sensor;
using climate::ClimateCall;
using climate::ClimatePreset;
using climate::ClimateTraits;
using climate::ClimateMode;
using midea_dongle::Frame;
using midea_dongle::ResponseStatus;
using midea_dongle::ResponseHandler;
using midea_dongle::MideaDongle;

class MideaAC : public midea_dongle::MideaAppliance, public climate::Climate, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }
  void on_frame(const Frame &frame) override;
  void on_idle() override { this->get_status_(); }
  void setup() override;
  bool can_proceed() override {
    return this->capabilities_.is_ready() || !this->use_autoconf_ || this->autoconf_failed_;
  }
  void dump_config() override;
  void set_dongle(MideaDongle *dongle) { this->dongle_ = dongle; }
  void set_outdoor_temperature_sensor(Sensor *sensor) { this->outdoor_sensor_ = sensor; }
  void set_humidity_setpoint_sensor(Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_power_sensor(Sensor *sensor) { this->power_sensor_ = sensor; }
  void set_beeper_feedback(bool state) { this->beeper_feedback_ = state; }
  bool allow_preset(ClimatePreset preset) const;
  void set_supported_modes(std::set<ClimateMode> modes) { this->supported_modes_ = std::move(modes); }
  void set_supported_swing_modes(std::set<ClimateSwingMode> modes) { this->supported_swing_modes_ = std::move(modes); }
  void set_supported_presets(std::set<ClimatePreset> presets) { this->supported_presets_ = std::move(presets); }
  void set_custom_presets(std::set<std::string> presets) { this->supported_custom_presets_ = std::move(presets); }
  void set_custom_fan_modes(std::set<std::string> modes) { this->supported_custom_fan_modes_ = std::move(modes); }
  void set_autoconf(bool value) { this->use_autoconf_ = value; }
  bool allow_custom_preset(const std::string &custom_preset) const;
  void do_follow_me(float temperature, bool beeper = false);
  void do_display_toggle();
  void do_swing_step();
  void do_beeper_on();
  void do_beeper_off();

 protected:
  void control(const ClimateCall &call) override;
  ClimateTraits traits() override;
  ResponseStatus read_status_(const Frame &frame);
  void get_power_usage_();
  void get_capabilities_();
  void get_status_();
  void display_toggle_();
#ifdef USE_REMOTE_TRANSMITTER
  void transmit_ir_(IrData &data) { this->dongle_->transmit_ir(data); }
#endif
  MideaDongle *dongle_{nullptr};
  Sensor *outdoor_sensor_{nullptr};
  Sensor *humidity_sensor_{nullptr};
  Sensor *power_sensor_{nullptr};
  std::set<ClimateMode> supported_modes_{};
  std::set<ClimateSwingMode> supported_swing_modes_{};
  std::set<ClimatePreset> supported_presets_{};
  std::set<std::string> supported_custom_presets_{};
  std::set<std::string> supported_custom_fan_modes_{};
  CommandFrame cmd_frame_{};
  Capabilities capabilities_{};
  bool use_autoconf_{false};
  bool autoconf_failed_{false};
  bool beeper_feedback_{false};
};

}  // namespace midea_ac
}  // namespace esphome
