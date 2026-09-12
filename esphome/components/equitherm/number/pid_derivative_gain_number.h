#pragma once

#include "equitherm_number_base.h"

namespace esphome::equitherm {

class EquithermClimate;

/// PID derivative gain (Kd) - anticipates future error
class PIDDerivativeGainNumber : public EquithermNumberBase, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;
};

}  // namespace esphome::equitherm
