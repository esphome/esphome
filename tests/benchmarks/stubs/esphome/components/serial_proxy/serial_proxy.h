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

namespace serial_proxy {

enum class SerialProxyResult : uint8_t {
  SERIAL_PROXY_RESULT_OK,
  SERIAL_PROXY_RESULT_ASSUMED_SUCCESS,
  SERIAL_PROXY_RESULT_PORT_IN_USE,
  SERIAL_PROXY_RESULT_INVALID_ARGUMENT,
  SERIAL_PROXY_RESULT_ERROR,
  SERIAL_PROXY_RESULT_TIMEOUT,
  SERIAL_PROXY_RESULT_NOT_SUPPORTED,
};

class SerialProxy {
 public:
  void set_instance_index(uint32_t index) { this->instance_index_ = index; }
  uint32_t get_instance_index() const { return this->instance_index_; }
  const char *get_name() const { return ""; }
  api::enums::SerialProxyPortType get_port_type() const { return {}; }
  api::APIConnection *get_api_connection() { return nullptr; }
  SerialProxyResult serial_proxy_request(api::APIConnection *conn, api::enums::SerialProxyRequestType type) {
    return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
  }
  SerialProxyResult configure(api::APIConnection *api_connection, uint32_t baudrate, bool flow_control, uint8_t parity,
                              uint8_t stop_bits, uint8_t data_size) {
    return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
  }
  void write_from_client(api::APIConnection *api_connection, const uint8_t *data, size_t len) {}
  SerialProxyResult set_modem_pins(api::APIConnection *api_connection, uint32_t line_states) {
    return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
  }
  uint32_t get_modem_pins() const { return 0; }
  uint32_t get_configured_modem_pins() const { return 0; }
  SerialProxyResult flush_port(api::APIConnection *api_connection) { return SerialProxyResult::SERIAL_PROXY_RESULT_OK; }

 protected:
  uint32_t instance_index_{0};
};

}  // namespace serial_proxy
}  // namespace esphome
