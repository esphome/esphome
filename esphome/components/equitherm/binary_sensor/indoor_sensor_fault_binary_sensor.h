#pragma once

#include "equitherm_binary_sensor_base.h"

namespace esphome::equitherm {

class EquithermClimate;

/// Indoor sensor fault indicator - ON when indoor sensor has failed and PID is disabled (pure equitherm mode)
class IndoorSensorFaultBinarySensor : public EquithermBinarySensorBase, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void update_from_parent_();
};

}  // namespace esphome::equitherm
