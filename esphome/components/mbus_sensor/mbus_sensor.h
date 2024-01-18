#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/mbus/mbus_frame.h"

namespace esphome {
namespace mbus_sensor {

class MBusSensor : public Component, public sensor::Sensor {
 public:
  MBusSensor(uint8_t data_index, float factor) {
    this->data_index_ = data_index;
    this->factor_ = factor;
  }

  // void publish(const mbus::MBusValue &value);
  int8_t get_data_index() const { return this->data_index_; };
  float get_factor() const { return this->factor_; };

 protected:
  float factor_;
  uint8_t data_index_;
};

}  // namespace mbus_sensor
}  // namespace esphome
