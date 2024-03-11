#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ESP_ADF
/* Tried and failed with:
   esp_peripherals/driver/i2c_bus/i2c_bus.h
   driver/i2c_bus/i2c_bus.h
   Note the conflicting i2c_bus.h header name that exists in both path
*/
#include "../components/esp_peripherals/driver/i2c_bus/i2c_bus.h"
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/component.h"
#include <driver/i2c.h>

namespace esphome {
namespace i2c {

class ADFI2CBus : public I2CBus, public Component {
 private:
  enum RecoveryCode {
    RECOVERY_FAILED_SCL_LOW,
    RECOVERY_FAILED_SDA_LOW,
    RECOVERY_COMPLETED,
  };

 public:
  void setup() override;
  void dump_config() override;
  ErrorCode readv(uint8_t address, ReadBuffer *buffers, size_t cnt) override;
  ErrorCode writev(uint8_t address, WriteBuffer *buffers, size_t cnt, bool stop) override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { scan_ = scan; }
  void set_sda_pin(uint8_t sda_pin) { sda_pin_ = sda_pin; }
  void set_sda_pullup_enabled(bool sda_pullup_enabled) { sda_pullup_enabled_ = sda_pullup_enabled; }
  void set_scl_pin(uint8_t scl_pin) { scl_pin_ = scl_pin; }
  void set_scl_pullup_enabled(bool scl_pullup_enabled) { scl_pullup_enabled_ = scl_pullup_enabled; }
  void set_frequency(uint32_t frequency) { frequency_ = frequency; }

 public:
  void recover_();
  RecoveryCode recovery_result_;

 protected:
  uint8_t sda_pin_;
  bool sda_pullup_enabled_;
  uint8_t scl_pin_;
  bool scl_pullup_enabled_;
  uint32_t frequency_;

  i2c_bus_handle_t handle_;
};

}  // namespace i2c
}  // namespace esphome

#endif  // USE_ESP_IDF
