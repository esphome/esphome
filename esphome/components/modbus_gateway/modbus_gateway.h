#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#include <cstdint>

namespace esphome::modbus_gateway {

static constexpr uint8_t MAX_PORTS = 4;
static constexpr uint16_t MAX_FRAME = 256;

/// UART the gateway exposes. A modbus hub writes a request here and reads the matching response.
class GatewayUart : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override { return this->rx_len_; }
  size_t available_for_write() override { return MAX_FRAME - this->tx_len_; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

  size_t take_tx(uint8_t *dest, size_t cap);
  bool push_rx(const uint8_t *data, size_t len);

 protected:
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

  uint16_t tx_len_{0};
  uint16_t rx_len_{0};
  bool trunc_logged_{false};
  uint8_t tx_[MAX_FRAME]{};
  uint8_t rx_[MAX_FRAME]{};
};

/// One Modbus RTU bus, several masters. One request is on the bus at a time.
/// The response is written back only to the port that sent the request.
class ModbusGateway : public Component, public uart::UARTDevice {
 public:
  void set_response_timeout(uint32_t ms) { this->response_timeout_ms_ = ms; }
  void set_cache_time(uint32_t ms) { this->cache_time_ms_ = ms; }
  void set_port_count(uint8_t count) { this->port_count_ = count; }
  void set_port_uart(uint8_t index, uart::UARTComponent *uart) { this->ports_[index].uart = uart; }
  void set_port_local(uint8_t index, GatewayUart *uart) { this->ports_[index].local = uart; }

  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

 protected:
  struct Port {
    uart::UARTComponent *uart{nullptr};
    GatewayUart *local{nullptr};
    uint32_t last_ms{0};
    uint16_t len{0};
    uint16_t pending_len{0};
    uint8_t data[MAX_FRAME]{};
    uint8_t pending[MAX_FRAME]{};
  };

  void read_port_(uint8_t index, uint32_t now);
  void take_requests_(uint8_t index, uint32_t now);
  void read_bus_(uint32_t now);
  bool start_next_(uint32_t now);
  bool write_frame_(uart::UARTComponent *dest, const uint8_t *data, uint16_t len);
  void deliver_(uint8_t index, const uint8_t *data, uint16_t len);
  bool serve_from_cache_(uint8_t index, const uint8_t *data, uint16_t len, uint32_t now);
  void store_cache_(const uint8_t *request, uint16_t request_len, const uint8_t *response, uint16_t response_len,
                    uint32_t now);
  void log_bad_(uint32_t now, bool crc);
  void log_dropped_(bool on_uart, uint16_t len);
  uart::UARTComponent *endpoint_(uint8_t index);

  Port ports_[MAX_PORTS]{};
  uint32_t response_timeout_ms_{500};
  uint32_t cache_time_ms_{0};
  uint32_t sent_ms_{0};
  uint32_t cache_ms_{0};
  uint32_t bus_last_ms_{0};
  uint32_t last_bad_log_ms_{0};
  uint32_t last_timeout_log_ms_{0};
  uint32_t last_response_log_ms_{0};
  uint16_t request_len_{0};
  uint16_t cache_len_{0};
  uint16_t bus_len_{0};
  uint8_t port_count_{0};
  uint8_t next_port_{0};
  int8_t active_{-1};
  bool cache_valid_{false};
  bool awaiting_{false};
  uint8_t request_[MAX_FRAME]{};
  uint8_t cache_key_[6]{};
  uint8_t cache_[MAX_FRAME]{};
  uint8_t bus_[MAX_FRAME]{};
};

}  // namespace esphome::modbus_gateway
