#pragma once
#include <cstdint>

namespace esphome::actuator {

// Inheritance: IActuator — pure interface

enum ActuatorOperation : uint8_t;

class IActuator {
 public:
  virtual float get_position() const = 0;
  virtual void set_position(float pos) = 0;
  virtual ActuatorOperation get_operation() const = 0;
  virtual void set_operation(ActuatorOperation op) = 0;
  virtual void do_publish_state(bool save = true) = 0;
};

}  // namespace esphome::actuator
