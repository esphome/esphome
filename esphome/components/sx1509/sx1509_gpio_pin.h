#pragma once

#include "sx1509.h"

#define BREATHE_OUTPUT 0x04

/// Modes for SX1509 pins
enum SX1509GPIOMode : uint8_t {
  SX1509_INPUT = INPUT,                   // 0x00
  SX1509_INPUT_PULLUP = INPUT_PULLUP,     // 0x02
  SX1509_OUTPUT = OUTPUT,                 // 0x01
  SX1509_BREATHE_OUTPUT = BREATHE_OUTPUT  // 0x04
};

namespace esphome {
namespace sx1509 {

class SX1509Component;

class SX1509GPIOPin : public GPIOPin {
 public:
  SX1509GPIOPin(SX1509Component *parent, uint8_t pin, uint8_t mode, bool inverted = false);

  void setup() override;
  void pin_mode(uint8_t mode) override;
  bool digital_read() override;
  void digital_write(bool value) override;

 protected:
  SX1509Component *parent_;
};

}}