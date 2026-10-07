#pragma once

#ifdef USE_HOST

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "uart_component.h"

namespace esphome::uart {

class HostUartComponent final : public UARTComponent, public Component {
 public:
  virtual ~HostUartComponent();
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override;
  UARTFlushResult flush() override;
  bool supports_modem_control() const override { return true; }
  void set_modem_control(bool dtr, bool rts) override;
  bool get_dtr() const override;
  bool get_rts() const override;
  void set_name(std::string port_name) { port_name_ = port_name; };

 protected:
  void update_error_(const std::string &error);
  int get_modem_bits_() const;
  void check_logger_conflict() override {}
  std::string port_name_;
  std::string first_error_;
  int file_descriptor_ = -1;
  bool has_peek_{false};
  uint8_t peek_byte_;
};

}  // namespace esphome::uart
#endif  // USE_HOST
