#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"

namespace esphome::copy {

class CopyNumber : public number::Number, public Component {
 public:
  void set_source(number::Number *source) { source_ = source; }
  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;

  number::Number *source_;
};

}  // namespace esphome::copy
