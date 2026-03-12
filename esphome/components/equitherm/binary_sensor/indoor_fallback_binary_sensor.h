#pragma once

#include "equitherm_binary_sensor_base.h"

namespace esphome {
namespace equitherm {

class EquithermClimate;

/// Indoor fallback active sensor - indicates when indoor sensor has failed and PID is disabled (pure equitherm mode)
class IndoorFallbackBinarySensor : public EquithermBinarySensorBase, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void update_from_parent_();
};

}  // namespace equitherm
}  // namespace esphome
