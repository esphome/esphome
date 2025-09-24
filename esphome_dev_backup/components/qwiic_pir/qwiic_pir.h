/*
 * Adds support for Qwiic PIR motion sensors that communicate over an I2C bus.
 * These sensors use Sharp PIR motion sensors to detect motion. A firmware running on an ATTiny84 translates the digital
 * output to I2C communications.
 * ATTiny84 firmware: https://github.com/sparkfun/Qwiic_PIR (acccessed July 2023)
 * SparkFun's Arduino library: https://github.com/sparkfun/SparkFun_Qwiic_PIR_Arduino_Library (accessed July 2023)
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace qwiic_pir {

// Qwiic PIR I2C Register Addresses
enum {
  QWIIC_PIR_CHIP_ID = 0x00,
  QWIIC_PIR_EVENT_STATUS = 0x03,
  QWIIC_PIR_DEBOUNCE_TIME = 0x05,  // uint16_t debounce time in milliseconds
};

enum DebounceMode {
  RAW_DEBOUNCE_MODE,
  NATIVE_DEBOUNCE_MODE,
  HYBRID_DEBOUNCE_MODE,
};

static const uint8_t QWIIC_PIR_DEVICE_ID = 0x72;

class QwiicPIRComponent : public Component, public i2c::I2CDevice, public binary_sensor::BinarySensor {
 public:
  void setup() override;
  void loop() override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_debounce_time(uint16_t debounce_time) { this->debounce_time_ = debounce_time; }
  void set_debounce_mode(DebounceMode mode) { this->debounce_mode_ = mode; }

 protected:
  uint16_t debounce_time_{};

  DebounceMode debounce_mode_{};

  enum ErrorCode {
    NONE = 0,
    ERROR_COMMUNICATION_FAILED,
    ERROR_WRONG_CHIP_ID,
  } error_code_{NONE};

  union {
    struct {
      bool raw_reading : 1;      // raw state of PIR sensor
      bool event_available : 1;  // a debounced object has been detected or removed
      bool object_removed : 1;   // a debounced object is no longer detected
      bool object_detected : 1;  // a debounced object has been detected
      bool : 4;
    };
    uint8_t reg;
  } event_register_ = {.reg = 0};

  void clear_events_();
};

}  // namespace qwiic_pir
}  // namespace esphome
