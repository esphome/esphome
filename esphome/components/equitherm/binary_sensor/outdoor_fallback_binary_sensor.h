#pragma once

#include "equitherm_binary_sensor_base.h"

namespace esphome {
namespace equitherm {

class EquithermClimate;

/// Outdoor fallback active sensor - indicates when outdoor sensor has failed and fallback temp is in use
class OutdoorFallbackBinarySensor : public EquithermBinarySensorBase, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void update_from_parent_();
};

}  // namespace equitherm
}  // namespace esphome
