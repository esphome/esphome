/*
 * Adds support for Qwiic PIR motion sensors that communicate over an I2C bus.
 * These sensors use Sharp PIR motion sensors to detect motion. A firmware running on an ATTiny84 translates the digital
 * output to I2C communications.
 * ATTiny84 firmware: https://github.com/sparkfun/Qwiic_PIR
 * SparkFun's Arduino library: https://github.com/sparkfun/SparkFun_Qwiic_PIR_Arduino_Library
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::qwiic_pir {

enum class DebounceMode : uint8_t {
  DEBOUNCE_MODE_RAW,
  DEBOUNCE_MODE_NATIVE,
  DEBOUNCE_MODE_HYBRID,
};

class QwiicPIRComponent final : public Component, public i2c::I2CDevice, public binary_sensor::BinarySensor {
 public:
  QwiicPIRComponent(DebounceMode debounce_mode, uint16_t debounce_time)
      : debounce_time_(debounce_time), debounce_mode_(debounce_mode) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  void clear_events_();

  uint16_t debounce_time_;
  DebounceMode debounce_mode_;
};

}  // namespace esphome::qwiic_pir
