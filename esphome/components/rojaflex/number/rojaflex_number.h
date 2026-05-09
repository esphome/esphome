#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"

#include "../rojaflex.h"

namespace esphome::rojaflex {

enum class RojaflexNumberType : uint8_t {
  TX_REPETITIONS,
};

class RojaflexNumber : public number::Number, public Component, public RojaflexDevice {
 public:
  void set_number_type(RojaflexNumberType type) { this->type_ = type; }
  void setup() override;
  void control(float value) override;

 protected:
  RojaflexNumberType type_{RojaflexNumberType::TX_REPETITIONS};
};

}  // namespace esphome::rojaflex
