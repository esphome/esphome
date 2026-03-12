#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace equitherm {

class EquithermClimate;

/// Control mode text sensor - exposes the current operating mode as a human-readable string.
/// Uses const char* literals to avoid heap allocation on state changes.
class ControlModeTextSensor : public text_sensor::TextSensor, public Component, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void update_from_parent_();
};

}  // namespace equitherm
}  // namespace esphome
