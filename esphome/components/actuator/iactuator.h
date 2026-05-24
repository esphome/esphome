#pragma once
#include <cstdint>
#include "esphome/core/optional.h"

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
  /// Restore previously saved state and return restored position, or empty if no saved state.
  virtual optional<float> do_restore_state() = 0;
  /// Return the name of the entity (for logging).
  virtual const char *get_entity_name() const = 0;
};

}  // namespace esphome::actuator
