#pragma once

#ifdef USE_ESP_IDF

#include "i2c_bus.h"
#include "esphome/core/component.h"
#include <driver/i2c.h>

namespace esphome {
namespace i2c {

class IDFI2CBus : public I2CBus, public Component {
 public:
  void setup() override;
  ErrorCode readv(uint8_t address, ReadBuffer *buffers, size_t cnt) override;
  ErrorCode writev(uint8_t address, WriteBuffer *buffers, size_t cnt) override;

 protected:
  i2c_port_t port_;
  uint8_t sda_pin_;
  bool sda_pullup_enabled_;
  uint8_t scl_pin_;
  bool scl_pullup_enabled_;
  uint32_t clock_speed_;
};

}  // namespace i2c
}  // namespace esphome

#endif // USE_ESP_IDF
