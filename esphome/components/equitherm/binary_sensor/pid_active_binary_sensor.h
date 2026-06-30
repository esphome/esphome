#pragma once

#include "equitherm_binary_sensor_base.h"

namespace esphome {
namespace equitherm {

class EquithermClimate;

/// PID active indicator - ON when PID controller gains are non-zero (actively correcting heating curve)
class PidActiveBinarySensor : public EquithermBinarySensorBase, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void update_from_parent_();
};

}  // namespace equitherm
}  // namespace esphome
