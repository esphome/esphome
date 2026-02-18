#pragma once

#ifdef USE_HOST

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "i2c_bus.h"

namespace esphome {
namespace i2c {

class HostI2CBus : public I2CBus, public Component {
 public:
  virtual ~HostI2CBus();

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override;

  void set_bus_num(uint8_t bus_num) { this->bus_num_ = bus_num; }
  void set_scan(bool scan) { this->scan_ = scan; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }

  uint8_t get_bus_num() const { return this->bus_num_; }

 protected:
  void update_error_(const std::string &error);
  ErrorCode map_errno_to_error_code_(int err);

  uint8_t bus_num_{0};
  uint32_t frequency_{50000};
  int file_descriptor_{-1};
  bool initialized_{false};
  std::string first_error_{""};
};

}  // namespace i2c
}  // namespace esphome

#endif  // USE_HOST
