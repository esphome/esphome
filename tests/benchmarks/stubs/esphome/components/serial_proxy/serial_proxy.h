// Stub for benchmark builds — provides the minimal interface that
// api_connection.cpp and Application need when USE_SERIAL_PROXY is defined,
// without pulling in the real UART implementation.
#pragma once

#include <cstdint>
#include <cstddef>
#include "esphome/components/api/api_pb2.h"

namespace esphome {

namespace api {
class APIConnection;
}  // namespace api

namespace uart {
enum class UARTFlushResult : uint8_t {
  UART_FLUSH_RESULT_SUCCESS,
  UART_FLUSH_RESULT_ASSUMED_SUCCESS,
  UART_FLUSH_RESULT_TIMEOUT,
  UART_FLUSH_RESULT_FAILED,
};
}  // namespace uart

namespace serial_proxy {

class SerialProxy {
 public:
  void set_instance_index(uint32_t index) { this->instance_index_ = index; }
  uint32_t get_instance_index() const { return this->instance_index_; }
  const char *get_name() const { return ""; }
  api::enums::SerialProxyPortType get_port_type() const { return {}; }
  api::APIConnection *get_api_connection() { return nullptr; }
  void serial_proxy_request(api::APIConnection *conn, api::enums::SerialProxyRequestType type) {}
  void configure(uint32_t baudrate, bool flow_control, uint8_t parity, uint32_t stop_bits, uint32_t data_size) {}
  void write_from_client(const uint8_t *data, size_t len) {}
  void set_modem_pins(uint32_t line_states) {}
  uint32_t get_modem_pins() const { return 0; }
  uart::UARTFlushResult flush_port() { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

 protected:
  uint32_t instance_index_{0};
};

}  // namespace serial_proxy
}  // namespace esphome
