#pragma once

#ifdef USE_ZEPHYR

#include "i2c_bus.h"
#include "esphome/core/component.h"

struct device;

namespace esphome {
namespace i2c {

class ZephyrI2CBus : public I2CBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  ErrorCode readv(uint8_t address, ReadBuffer *buffers, size_t cnt) override;
  ErrorCode writev(uint8_t address, WriteBuffer *buffers, size_t cnt, bool stop) override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { scan_ = scan; }

 protected:
  const struct device *i2c_dev_ = nullptr;
  int recovery_result_ = 0;
};

}  // namespace i2c
}  // namespace esphome

#endif  // USE_ESP_IDF
