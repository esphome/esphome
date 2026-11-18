#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::resistance {

enum ResistanceConfiguration {
  UPSTREAM,
  DOWNSTREAM,
};

class ResistanceSensor final : public Component, public sensor::Sensor {
  SUB_SENSOR(source)

 public:
  void set_configuration(ResistanceConfiguration configuration) { configuration_ = configuration; }
  void set_resistor(float resistor) { resistor_ = resistor; }
  void set_reference_voltage(float reference_voltage) { reference_voltage_ = reference_voltage; }

  void setup() override {
    this->source_sensor_->add_on_state_callback([this](float value) { this->process_(value); });
    if (this->source_sensor_->has_state())
      this->process_(this->source_sensor_->state);
  }
  void dump_config() override;

 protected:
  void process_(float value);
  ResistanceConfiguration configuration_;
  float resistor_;
  float reference_voltage_;
};

}  // namespace esphome::resistance
