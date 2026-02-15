#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace modem {

class ModemNMEAUARTComponent : public uart::UARTComponent, public PollingComponent {
 public:
  void set_gnss_command(const std::string &gnss_command) { this->gnss_command_ = gnss_command; }

  // UART read/write methods
  void write_array(const uint8_t *data, size_t len) override{};

  bool peek_byte(uint8_t *data) override { return false; };

  bool read_array(uint8_t *data, size_t len) override;

  size_t available() override { return this->nmea_buffer_size_ - this->read_ptr_; }

  void flush() override {}

  void check_logger_conflict() override {}

  // PollingComponent methods

  void update() override;

 protected:
  uint8_t nmea_buffer_[168] = {0};  // max nmea length is 82, we use 168 to accommodate both GPGGA and GPRMC
  size_t nmea_buffer_size_{0};
  size_t read_ptr_{0};

  std::string gnss_command_;
};

}  // namespace modem
}  // namespace esphome
