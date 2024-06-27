#pragma once
#include "esphome/components/mbus/mbus_frame.h"
#include "esphome/components/mbus/mbus_sensor_base.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace mbus_sensor {

class MBusSensor : public mbus::MBusSensorBase, public Component, public sensor::Sensor {
 public:
  MBusSensor(uint8_t data_index, float factor) {
    this->data_index_ = data_index;
    this->factor_ = factor;
  }

  void publish(const std::unique_ptr<mbus::MBusValue> &value) override;
};

}  // namespace mbus_sensor
}  // namespace esphome
