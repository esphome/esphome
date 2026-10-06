#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/components/uart/uart_virtual.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <cstdint>

namespace esphome::modbus_gateway {

static constexpr uint16_t MAX_FRAME = 256;

class ModbusGateway;

/// A UART for a modbus hub on this device. A write goes straight into the gateway's port buffer, the matching
/// response comes back through inject_rx().
class GatewayUart final : public uart::VirtualUARTComponent {
 public:
  GatewayUart() : VirtualUARTComponent(MAX_FRAME) {}
  void attach(ModbusGateway *gateway, uint8_t index) {
    this->gateway_ = gateway;
    this->index_ = index;
  }

  void write_array(const uint8_t *data, size_t len) override;
  size_t available_for_write() override;
  uart::UARTFlushResult flush() override;

 protected:
  ModbusGateway *gateway_{nullptr};
  uint8_t index_{0};
};

/// One Modbus RTU bus, several clients. One request is on the bus at a time.
/// The response is written back only to the port that sent the request.
class ModbusGateway : public Component, public uart::UARTDevice {
 public:
  void set_response_timeout(uint32_t ms) { this->response_timeout_ms_ = ms; }
  void set_port_count(uint8_t count) { this->port_count_ = count; }
  void set_port_uart(uint8_t index, uart::UARTComponent *uart) { this->ports_[index].uart = uart; }
  void set_port_local(uint8_t index, GatewayUart *uart) {
    this->ports_[index].uart = uart;
    this->ports_[index].local = true;
    uart->attach(this, index);
  }
  /// A local port's hub wrote a block. It goes into that port's buffer.
  void port_write(uint8_t index, const uint8_t *data, size_t len);
  size_t port_room(uint8_t index) const { return MAX_FRAME - this->ports_[index].len; }
  bool port_empty(uint8_t index) const { return this->ports_[index].len == 0; }

  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

 protected:
  // data holds the request that waits for the bus first (pending_len bytes), then what the client sent after it.
  struct Port {
    uart::UARTComponent *uart{nullptr};
    uint32_t last_ms{0};
    uint16_t len{0};
    uint16_t pending_len{0};
    bool local{false};
    uint8_t data[MAX_FRAME]{};
  };

  void run_(uint32_t now);
  void read_port_(uint8_t index, uint32_t now);
  void take_requests_(uint8_t index, uint32_t now);
  void read_bus_(uint32_t now);
  void drain_bus_(uint32_t now);
  bool start_next_(uint32_t now);
  bool write_frame_(uart::UARTComponent *dest, const uint8_t *data, uint16_t len);
  bool deliver_(uint8_t index, const uint8_t *data, uint16_t len, uint32_t now);
  void log_bad_(uint32_t now, bool crc);
  void log_mismatch_(uint32_t now);
  void log_dropped_(uint32_t now, bool on_uart, uint16_t len);

  Port ports_[MODBUS_GATEWAY_PORT_COUNT]{};
  uint32_t response_timeout_ms_{500};
  uint32_t sent_ms_{0};
  uint32_t bus_last_ms_{0};
  uint32_t last_bad_log_ms_{0};
  uint32_t last_mismatch_log_ms_{0};
  uint32_t last_timeout_log_ms_{0};
  uint32_t last_response_log_ms_{0};
  uint16_t bus_len_{0};
  uint8_t port_count_{0};
  uint8_t next_port_{0};
  int8_t active_{-1};
  bool awaiting_{false};
  // Set after a timeout. A late response must not reach the next port.
  bool quarantine_{false};
  // Unit, function, start and quantity of the request on the bus.
  uint8_t request_key_[6]{};
  uint8_t bus_[MAX_FRAME]{};
};

}  // namespace esphome::modbus_gateway
