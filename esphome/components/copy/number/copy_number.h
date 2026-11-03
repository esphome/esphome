#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"

namespace esphome::copy {

class CopyNumber final : public number::Number, public Component {
  SUB_NUMBER(source)

 public:
  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;
};

}  // namespace esphome::copy
