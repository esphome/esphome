#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace resistance {

enum ResistanceConfiguration {
  UPSTREAM,
  DOWNSTREAM,
};

class ResistanceSensor : public Component, public sensor::Sensor {
 public:
  void set_sensor(Sensor *sensor) { sensor_ = sensor; }
  void set_configuration(ResistanceConfiguration configuration) { configuration_ = configuration; }
  void set_resistor(float resistor) { resistor_ = resistor; }
  void set_reference_voltage(float reference_voltage) { reference_voltage_ = reference_voltage; }

  void setup() override {
    this->sensor_->add_on_state_callback([this](float value) { this->process_(value); });
    if (this->sensor_->has_state())
      this->process_(this->sensor_->state);
  }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void process_(float value);
  sensor::Sensor *sensor_;
  ResistanceConfiguration configuration_;
  float resistor_;
  float reference_voltage_;
};

}  // namespace resistance
}  // namespace esphome
