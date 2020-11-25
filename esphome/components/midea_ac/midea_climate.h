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
  void set_beeper_feedback(bool state) { this->beeper_feedback_ = state; }
  

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  const QueryFrame query_frame_;
  CommandFrame cmd_frame_;
  bool ctrl_request_{false};
  bool beeper_feedback_{false};
  midea_dongle::MideaDongle *parent_{nullptr};
  sensor::Sensor *outdoor_sensor_{nullptr};
};

}  // namespace midea_ac
}  // namespace esphome
