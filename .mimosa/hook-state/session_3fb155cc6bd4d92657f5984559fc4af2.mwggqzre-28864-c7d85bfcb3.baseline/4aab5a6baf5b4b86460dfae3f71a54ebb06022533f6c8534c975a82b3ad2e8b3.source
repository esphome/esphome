#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::ntc {

class NTC final : public Component, public sensor::Sensor {
  SUB_SENSOR(source)

 public:
  void set_a(double a) { a_ = a; }
  void set_b(double b) { b_ = b; }
  void set_c(double c) { c_ = c; }
  void setup() override;
  void dump_config() override;

 protected:
  void process_(float value);

  double a_;
  double b_;
  double c_;
};

}  // namespace esphome::ntc
