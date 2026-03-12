#pragma once

#include "equitherm_number_base.h"

namespace esphome {
namespace equitherm {

class EquithermClimate;

/// Rate limit - controls how fast the flow temperature can change (°C per minute)
class RateLimitNumber : public EquithermNumberBase, public Parented<EquithermClimate> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;
};

}  // namespace equitherm
}  // namespace esphome
