#pragma once

#ifdef USE_HOST

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "uart_component.h"

namespace esphome {
namespace uart {

class HostUartComponent : public UARTComponent, public Component {
 public:
  virtual ~HostUartComponent();
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  int available() override;
  void flush() override;
  void set_name(std::string port_name) { port_name_ = port_name; };

 protected:
  void update_error_(const std::string &error);
  void check_logger_conflict() override {}
  std::string port_name_;
  std::string first_error_{""};
  int file_descriptor_ = -1;
  bool has_peek_{false};
  uint8_t peek_byte_;
};

}  // namespace uart
}  // namespace esphome

#endif  // USE_HOST
