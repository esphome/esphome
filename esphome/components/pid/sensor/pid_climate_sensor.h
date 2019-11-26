#pragma once

#include "esphome/core/component.h"
#include "esphome/components/pid/pid_climate.h"

namespace esphome {
namespace pid {

class PIDClimateSensor : public sensor::Sensor, public Component {
 public:
  void setup() override;
  void set_parent(PIDClimate *parent) { parent_ = parent; }

  void dump_config() override;

 protected:
  void update_from_parent_();
  PIDClimate *parent_;
};

}  // namespace pid
}  // namespace esphome
