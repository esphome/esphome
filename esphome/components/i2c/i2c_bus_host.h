#pragma once

#ifdef USE_HOST

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "i2c_bus.h"

namespace esphome::i2c {

class HostI2CBus final : public I2CBus, public Component {
 public:
  ~HostI2CBus() override;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override;

  void set_device(const std::string &device) { this->device_ = device; }
  void set_scan(bool scan) { this->scan_ = scan; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }

  const std::string &get_device() const { return this->device_; }

 protected:
  void update_error_(const std::string &error);
  ErrorCode map_errno_to_error_code_(int err);

  std::string device_;
  uint32_t frequency_{50000};
  int file_descriptor_{-1};
  bool initialized_{false};
  std::string first_error_;
};

}  // namespace esphome::i2c

#endif  // USE_HOST
