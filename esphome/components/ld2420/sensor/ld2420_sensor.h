#pragma once

#include "../ld2420.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ld2420 {

class LD2420Sensor : public LD2420Listener, public Component, sensor::Sensor {
 public:
  void dump_config() override;
  void set_distance_sensor(sensor::Sensor *sensor) { this->distance_sensor_ = sensor; }
  void on_distance(uint16_t distance) override {
    if (this->distance_sensor_ != nullptr) {
      if (this->distance_sensor_->get_state() != distance) {
        this->distance_sensor_->publish_state(distance);
      }
    }
  }
  void on_energy(uint16_t *gate_energy, size_t size) override {
    for (size_t active = 0; active < size; active++) {
      if (this->energy_sensors_[active] != nullptr) {
        this->energy_sensors_[active]->publish_state(gate_energy[active]);
      }
    }
  }

 protected:
  sensor::Sensor *distance_sensor_{nullptr};
  std::vector<sensor::Sensor *> energy_sensors_ = std::vector<sensor::Sensor *>(LD2420_TOTAL_GATES);
};

}  // namespace ld2420
}  // namespace esphome
