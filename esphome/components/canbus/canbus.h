#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace canbus {


class CanbusSensor {
public:
  void set_can_id(int can_id) { this->can_id_ = can_id; }

 private:
  int can_id_{0};
};

class CanbusBinarySensor : public CanbusSensor , public binary_sensor::BinarySensor {
  friend class Canbus;
 
};

class Canbus : public Component {
 public:
  Canbus(){};
  Canbus(const std::string &name){};
  virtual void send(int can_id, uint8_t *data);
  void register_can_device(CanbusSensor *component){};
};
}  // namespace canbus
}  // namespace esphome