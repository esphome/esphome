#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/components/uart/uart_virtual.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstdint>

namespace esphome::modbus_gateway {

static constexpr uint16_t MAX_FRAME = 256;
// The longest response to a read: 2000 coils or 125 registers are 250 data bytes.
static constexpr uint16_t MAX_READ_RESPONSE = 255;

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

/// One Modbus RTU bus, several clients, one request on the bus at a time; the response goes back to the port that
/// sent it. A request starts after 3.5 quiet characters; a frame larger than a FIFO goes out in parts without a pause.
class ModbusGateway : public Component, public uart::UARTDevice {
 public:
  void set_response_timeout(uint32_t ms) { this->response_timeout_us_ = ms * 1000; }
  void set_cache_time(uint32_t ms) { this->cache_time_ms_ = ms; }
  void set_cache_entries(uint16_t count) { this->cache_count_ = count; }
  void set_port_cache(uint8_t index, bool use) { this->ports_[index].use_cache = use; }
  void set_port_count(uint8_t count) { this->port_count_ = count; }
  /// The bus's bytes do not keep the timing of a serial line (TCP, USB, a virtual UART).
  void set_bus_unclocked() { this->bus_clocked_ = false; }
  void set_port_uart(uint8_t index, uart::UARTComponent *uart, bool clocked) {
    this->ports_[index].uart = uart;
    this->ports_[index].clocked = clocked;
  }
  void set_port_local(uint8_t index, GatewayUart *uart) {
    this->ports_[index].uart = uart;
    this->ports_[index].local = true;
    uart->attach(this, index);
  }
  /// A local port's hub wrote a block. It goes into that port's buffer.
  void port_write(uint8_t index, const uint8_t *data, size_t len);
  size_t port_room(uint8_t index) const { return MAX_FRAME - this->ports_[index].len; }
  bool port_empty(uint8_t index) const { return this->ports_[index].len == 0; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

 protected:
  enum class Phase : uint8_t {
    PHASE_IDLE,
    PHASE_SENDING,     // the request goes out to the bus
    PHASE_AWAITING,    // the response is read from the bus
    PHASE_TURNAROUND,  // after a broadcast, which nothing answers
    PHASE_DELIVERING,  // the response goes out to the port
  };
  enum class Write : uint8_t { WRITE_MORE, WRITE_DONE, WRITE_DOWN, WRITE_STALLED };

  // data holds the request that waits for the bus first (pending_len bytes), then what the client sent after it.
  struct Port {
    uart::UARTComponent *uart{nullptr};
    uint32_t last_us{0};
    // The quiet time that ends a frame from this port, and the longer one after a full driver batch (0: none).
    uint32_t gap_us{0};
    uint32_t batch_us{0};
    uint16_t len{0};
    uint16_t pending_len{0};
    bool local{false};
    bool clocked{true};
    bool batched{false};
    bool use_cache{false};
    uint8_t data[MAX_FRAME]{};
  };

  // The whole response to one read. key is the request's unit, function, start and quantity; used orders the
  // entries by their last use, so the one used longest ago gives way.
  struct CacheSlot {
    uint32_t stored_ms{0};
    uint32_t used{0};
    uint16_t len{0};
    uint8_t key[6]{};
    uint8_t data[MAX_READ_RESPONSE]{};
  };

  void run_(uint32_t now, uint32_t now_ms);
  void read_port_(uint8_t index, uint32_t now);
  void take_requests_(uint8_t index, uint32_t now);
  bool sending_(uint8_t index) const { return this->phase_ == Phase::PHASE_SENDING && this->active_ == index; }
  void start_next_(uint32_t now, uint32_t now_ms);
  void send_request_(uint32_t now);
  void read_bus_(uint32_t now, uint32_t now_ms);
  void take_response_(uint16_t len, uint32_t now);
  void send_response_(uint32_t now);
  void drain_bus_(uint32_t now);
  Write write_more_(uart::UARTComponent *dest, const uint8_t *data, uint32_t now);
  uint32_t bus_wait_(uint32_t now) const;
  void pace_(uint32_t now);
  bool serve_cached_(uint8_t index, uint32_t now, uint32_t now_ms);
  void store_cache_(uint16_t len, uint32_t now_ms);
  void clear_cache_(uint8_t unit);
  void log_bad_(bool crc);
  void log_mismatch_();
  void log_drop_(const LogString *why, size_t len);

  Port ports_[MODBUS_GATEWAY_PORT_COUNT]{};
  // cache_count_ entries in one block, allocated in setup().
  CacheSlot *slots_{nullptr};
  uint32_t cache_time_ms_{0};
  uint32_t cache_seq_{0};
  uint32_t last_evict_log_ms_{0};
  uint32_t response_timeout_us_{500000};
  // From setup(): the wire time of a character on the bus (0 without a clock), the quiet time before a request,
  // the one that ends a received frame, the one after a full driver batch and the one after a timeout.
  uint32_t bus_char_us_{0};
  uint32_t bus_tx_gap_us_{0};
  uint32_t bus_rx_gap_us_{0};
  uint32_t bus_batch_us_{0};
  uint32_t bus_closed_us_{0};
  uint32_t bus_last_us_{0};
  // The bus line is busy for tx_busy_us_ from tx_start_us_ with what was written.
  uint32_t tx_start_us_{0};
  uint32_t tx_busy_us_{0};
  // The open request ends wait_us_ after sent_us_: its wire time plus the response timeout or the turnaround.
  uint32_t sent_us_{0};
  uint32_t wait_us_{0};
  // The last time a frame in parts made progress.
  uint32_t progress_us_{0};
  uint32_t last_bad_log_ms_{0};
  uint32_t last_mismatch_log_ms_{0};
  uint32_t last_timeout_log_ms_{0};
  uint32_t last_drop_log_ms_{0};
  uint16_t bus_len_{0};
  uint16_t cache_count_{0};
  // Stored reads that gave way while they were still fresh, since the last log.
  uint16_t cache_evicted_{0};
  // The frame that goes out in parts: out_sent_ of out_len_ bytes are written.
  uint16_t out_len_{0};
  uint16_t out_sent_{0};
  uint8_t port_count_{0};
  uint8_t next_port_{0};
  uint8_t active_{0};
  Phase phase_{Phase::PHASE_IDLE};
  HighFrequencyLoopRequester fast_;
  bool bus_clocked_{true};
  bool bus_batched_{false};
  // Set after a timeout. A late response must not reach the next port.
  bool quarantine_{false};
  // Unit, function, start and quantity of the request on the bus.
  uint8_t request_key_[6]{};
  uint8_t bus_[MAX_FRAME]{};
};

}  // namespace esphome::modbus_gateway
