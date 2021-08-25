#pragma once
#include <Appliance/AirConditioner/AirConditioner.h>
#include "appliance_base.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace midea {

using sensor::Sensor;
using climate::ClimateCall;

class AirConditioner : public ApplianceBase<dudanov::midea::ac::AirConditioner> {
 public:
  void dump_config() override;
  void set_outdoor_temperature_sensor(Sensor *sensor) { this->outdoor_sensor_ = sensor; }
  void set_humidity_setpoint_sensor(Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_power_sensor(Sensor *sensor) { this->power_sensor_ = sensor; }
  void on_status_change() override;

  /* ############### */
  /* ### ACTIONS ### */
  /* ############### */

  void do_follow_me(float temperature, bool beeper = false);
  void do_display_toggle();
  void do_swing_step();
  void do_beeper_on() { this->set_beeper_feedback(true); }
  void do_beeper_off() { this->set_beeper_feedback(false); }
  void do_power_on() { this->base_.setPowerState(true); }
  void do_power_off() { this->base_.setPowerState(false); }

 protected:
  void control(const ClimateCall &call) override;
  ClimateTraits traits() override;
  Sensor *outdoor_sensor_{nullptr};
  Sensor *humidity_sensor_{nullptr};
  Sensor *power_sensor_{nullptr};
};

}  // namespace midea
}  // namespace esphome
