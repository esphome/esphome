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

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  const QueryFrame query_frame_;
  const PowerQueryFrame power_frame_;
  CommandFrame cmd_frame_;
  midea_dongle::MideaDongle *parent_;
  sensor::Sensor *outdoor_sensor_;
  sensor::Sensor *humidity_sensor_;
  sensor::Sensor *power_sensor_;
  uint8_t request_num_;
  bool ctrl_request_;
  bool beeper_feedback_;
  bool traits_swing_horizontal_;
  bool traits_swing_both_;
};

}  // namespace midea_ac
}  // namespace esphome
