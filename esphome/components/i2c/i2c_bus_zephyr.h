#pragma once

#ifdef USE_ZEPHYR

#include "i2c_bus.h"
#include "esphome/core/component.h"

#ifdef USE_ZEPHYR_VARIANT_NATIVE_SIM
#include <string>
#endif

struct device;  // NOLINT(readability-identifier-naming) - forward decl of Zephyr's device type

namespace esphome::i2c {

class ZephyrI2CBus final : public InternalI2CBus, public Component {
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

#ifdef USE_ZEPHYR_VARIANT_NATIVE_SIM
  void set_linux_bus(const std::string &linux_bus) { this->linux_bus_ = linux_bus; }
#endif

  int get_port() const override { return 0; }
  const device *get_i2c_dev() const { return this->i2c_dev_; }

 protected:
  const device *i2c_dev_;
  int recovery_result_ = 0;
  uint8_t sda_pin_{};
  uint8_t scl_pin_{};
  uint32_t dev_config_{};

#ifdef USE_ZEPHYR_VARIANT_NATIVE_SIM
  std::string linux_bus_{};
  int physical_fd_{-1};
#endif
};

}  // namespace esphome::i2c

#endif
