#pragma once

#include "sx1509.h"

namespace esphome {
namespace sx1509 {

class SX1509Component;

class SX1509GPIOPin : public GPIOPin {
 public:
  SX1509GPIOPin(SX1509Component *parent, uint8_t pin, uint8_t mode, bool inverted = false)
      : GPIOPin(pin, mode, inverted), parent_(parent){};
  void setup() override;
  void pin_mode(uint8_t mode) override;
  bool digital_read() override;
  void digital_write(bool value) override;

 protected:
  SX1509Component *parent_;
};

}  // namespace sx1509
}  // namespace esphome
