#pragma once

#include "equitherm_number_base.h"

namespace esphome::equitherm {

class EquithermClimate;

/// Rate limit for rising flow temperature (°C per minute) - boiler protection
/// Controls how fast the flow temperature can increase to prevent thermal shock
class RateLimitRisingNumber : public EquithermNumberBase, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;
};

}  // namespace esphome::equitherm
