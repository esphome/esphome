#pragma once

#include "equitherm_binary_sensor_base.h"

namespace esphome::equitherm {

class EquithermClimate;

/// Outdoor sensor fault indicator - ON when outdoor sensor is stale or reading out of range
class OutdoorSensorFaultBinarySensor : public EquithermBinarySensorBase, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void update_from_parent_();
};

}  // namespace esphome::equitherm
