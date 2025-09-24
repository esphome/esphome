#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ntc {

class NTC : public Component, public sensor::Sensor {
 public:
  void set_sensor(Sensor *sensor) { sensor_ = sensor; }
  void set_a(double a) { a_ = a; }
  void set_b(double b) { b_ = b; }
  void set_c(double c) { c_ = c; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  void process_(float value);

  sensor::Sensor *sensor_;
  double a_;
  double b_;
  double c_;
};

}  // namespace ntc
}  // namespace esphome
