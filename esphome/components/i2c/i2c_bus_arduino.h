#pragma once

#ifdef USE_ARDUINO

#include "i2c_bus.h"
#include "esphome/core/component.h"
#include <Wire.h>

namespace esphome {
namespace i2c {

enum RecoveryCode {
  RECOVERY_FAILED_SCL_LOW,
  RECOVERY_FAILED_SDA_LOW,
  RECOVERY_COMPLETED,
};

class ArduinoI2CBus : public I2CBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  ErrorCode readv(uint8_t address, ReadBuffer *buffers, size_t cnt) override;
  ErrorCode writev(uint8_t address, WriteBuffer *buffers, size_t cnt) override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { scan_ = scan; }
  void set_sda_pin(uint8_t sda_pin) { sda_pin_ = sda_pin; }
  void set_scl_pin(uint8_t scl_pin) { scl_pin_ = scl_pin; }
  void set_frequency(uint32_t frequency) { frequency_ = frequency; }

 private:
  void recover_();
  RecoveryCode recovery_result_;

 protected:
  TwoWire *wire_;
  bool scan_;
  uint8_t sda_pin_;
  uint8_t scl_pin_;
  uint32_t frequency_;
  bool initialized_ = false;
};

}  // namespace i2c
}  // namespace esphome

#endif  // USE_ARDUINO
