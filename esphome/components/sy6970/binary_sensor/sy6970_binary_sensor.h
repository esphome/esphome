#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "../sy6970.h"

namespace esphome {
namespace sy6970 {

class SY6970BinarySensor : public PollingComponent {
 public:
  void set_parent(SY6970Component *parent) { this->parent_ = parent; }
  void set_vbus_connected_binary_sensor(binary_sensor::BinarySensor *vbus_connected_binary_sensor) {
    this->vbus_connected_binary_sensor_ = vbus_connected_binary_sensor;
  }
  void set_charging_binary_sensor(binary_sensor::BinarySensor *charging_binary_sensor) {
    this->charging_binary_sensor_ = charging_binary_sensor;
  }
  void set_charge_done_binary_sensor(binary_sensor::BinarySensor *charge_done_binary_sensor) {
    this->charge_done_binary_sensor_ = charge_done_binary_sensor;
  }

  void update() override;

 protected:
  SY6970Component *parent_{nullptr};
  binary_sensor::BinarySensor *vbus_connected_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *charging_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *charge_done_binary_sensor_{nullptr};
};

}  // namespace sy6970
}  // namespace esphome
