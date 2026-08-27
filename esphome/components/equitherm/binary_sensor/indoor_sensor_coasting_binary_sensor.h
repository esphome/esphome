#pragma once

#include "equitherm_binary_sensor_base.h"

namespace esphome::equitherm {

class EquithermClimate;

/// Indoor sensor coasting indicator - ON when indoor data is quiet-but-plausible
/// (PID off, pure equitherm). Informational, not an alarm.
class IndoorSensorCoastingBinarySensor : public EquithermBinarySensorBase, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void update_from_parent_();
};

}  // namespace esphome::equitherm
