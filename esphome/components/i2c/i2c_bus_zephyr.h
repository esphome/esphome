#pragma once

#ifdef USE_ZEPHYR

#include "i2c_bus.h"
#include "esphome/core/component.h"

struct device;

namespace esphome::i2c {

class ZephyrI2CBus : public InternalI2CBus, public Component {
 public:
  explicit ZephyrI2CBus(const device *i2c_dev) : i2c_dev_(i2c_dev) {}
  void setup() override;
  void dump_config() override;
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { scan_ = scan; }
  void set_sda_pin(uint8_t sda_pin) { this->sda_pin_ = sda_pin; }
  void set_scl_pin(uint8_t scl_pin) { this->scl_pin_ = scl_pin; }
  void set_frequency(uint32_t frequency);

  int get_port() const override { return 0; }

 protected:
  const device *i2c_dev_;
  int recovery_result_ = 0;
  uint8_t sda_pin_{};
  uint8_t scl_pin_{};
  uint32_t dev_config_{};
};

}  // namespace esphome::i2c

#endif
