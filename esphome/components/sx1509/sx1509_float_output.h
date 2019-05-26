#pragma once

#include "sx1509.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace sx1509 {

class SX1509Component;

class SX1509FloatOutputChannel : public output::FloatOutput {
 public:
  SX1509FloatOutputChannel(SX1509Component *parent, uint8_t pin) : parent_(parent), pin_(pin) {}
  void setup_channel();

 protected:
  void write_state(float state) override;

  SX1509Component *parent_;
  uint8_t pin_;
};
}  // namespace sx1509
}  // namespace esphome