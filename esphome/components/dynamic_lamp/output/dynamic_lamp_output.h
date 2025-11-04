#pragma once

#include "../dynamic_lamp.h"

namespace esphome {
namespace dynamic_lamp {

class DynamicLamp : public output::FloatOutput, public Parented<DynamicLampComponent> {
 public:
 DynamicLamp(DynamicLampComponent *parent, DynamicLampIdx lamp) : parent_(parent), lamp_(lamp) {}

 protected:
  void write_state(float state) override;
  DynamicLampComponent *parent_;
  DynamicLampIdx lamp_;
  float state_;
};

}  // namespace dynamic_lamp
}  // namespace esphome
