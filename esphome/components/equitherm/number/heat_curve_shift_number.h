#pragma once

#include "equitherm_number_base.h"

namespace esphome::equitherm {

class EquithermClimate;

/// Heat curve shift - parallel displacement of the heating curve
class HeatCurveShiftNumber : public EquithermNumberBase, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;
};

}  // namespace esphome::equitherm
