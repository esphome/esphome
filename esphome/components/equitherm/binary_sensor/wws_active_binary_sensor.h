#pragma once

#include "equitherm_binary_sensor_base.h"

namespace esphome::equitherm {

class EquithermClimate;

/// WWS active sensor - indicates when warm weather shutdown suppresses heating
class WwsActiveBinarySensor : public EquithermBinarySensorBase, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void update_from_parent_();
};

}  // namespace esphome::equitherm
