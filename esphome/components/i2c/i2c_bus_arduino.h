#pragma once

#ifdef USE_ARDUINO

#include <Wire.h>
#include "esphome/core/component.h"
#include "i2c_bus.h"

namespace esphome {
namespace i2c {

enum RecoveryCode {
  RECOVERY_FAILED_SCL_LOW,
  RECOVERY_FAILED_SDA_LOW,
  RECOVERY_COMPLETED,
};

class ArduinoI2CBus : public InternalI2CBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  ErrorCode readv(uint8_t address, ReadBuffer *buffers, size_t cnt) override;
  ErrorCode writev(uint8_t address, WriteBuffer *buffers, size_t cnt, bool stop) override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { scan_ = scan; }
  void set_sda_pin(uint8_t sda_pin) { sda_pin_ = sda_pin; }
  void set_scl_pin(uint8_t scl_pin) { scl_pin_ = scl_pin; }
  void set_frequency(uint32_t frequency) { frequency_ = frequency; }
  void set_timeout(uint32_t timeout) { timeout_ = timeout; }

  int get_port() const override { return this->port_; }

 private:
  void recover_();
  void set_pins_and_clock_();
  RecoveryCode recovery_result_;

 protected:
  int8_t port_{-1};
  TwoWire *wire_;
  uint8_t sda_pin_;
  uint8_t scl_pin_;
  uint32_t frequency_;
  uint32_t timeout_ = 0;
  bool initialized_ = false;
};

}  // namespace i2c
}  // namespace esphome

#endif  // USE_ARDUINO
