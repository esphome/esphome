#pragma once

#if defined(USE_ARDUINO) && !defined(USE_ESP32) && !defined(USE_LIBRETINY)

#include <Wire.h>
#include "esphome/core/component.h"
#include "i2c_bus.h"

namespace esphome::i2c {

enum RecoveryCode {
  RECOVERY_FAILED_SCL_LOW,
  RECOVERY_FAILED_SDA_LOW,
  RECOVERY_COMPLETED,
};

class ArduinoI2CBus final : public InternalI2CBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override {
    return this->transfer_(address, nullptr, 0, write_buffer, write_count, read_buffer, read_count);
  }
  ErrorCode write_prefixed(uint8_t address, const uint8_t *prefix, size_t prefix_len, const uint8_t *data,
                           size_t len) override {
    return this->transfer_(address, prefix, prefix_len, data, len, nullptr, 0);
  }
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { this->scan_ = scan; }
  void set_sda_pin(uint8_t sda_pin) { this->sda_pin_ = sda_pin; }
  void set_scl_pin(uint8_t scl_pin) { this->scl_pin_ = scl_pin; }
  void set_timeout(uint32_t timeout) { this->timeout_ = timeout; }

  int get_port() const override { return 0; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
#ifdef I2C_PORT_FREQUENCY_COUNT
  ErrorCode switch_frequency(uint32_t frequency) override;
  uint32_t get_frequency() const override { return this->frequency_; }
#endif

 private:
  /// Writes `prefix` then `write_buffer` in one transaction, then reads
  ErrorCode transfer_(uint8_t address, const uint8_t *prefix, size_t prefix_len, const uint8_t *write_buffer,
                      size_t write_count, uint8_t *read_buffer, size_t read_count);
  void recover_();
  void set_pins_and_clock_();
  RecoveryCode recovery_result_;

 protected:
  TwoWire *wire_;
  uint8_t sda_pin_;
  uint8_t scl_pin_;
  uint32_t frequency_;
  uint32_t timeout_ = 0;
  bool initialized_ = false;
};

}  // namespace esphome::i2c

#endif  // defined(USE_ARDUINO) && !defined(USE_ESP32)
