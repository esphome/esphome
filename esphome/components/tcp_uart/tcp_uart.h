#pragma once

#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/components/socket/tcp_client_link.h"
#ifdef USE_SOCKET_TCP_LISTENER
#include "esphome/components/socket/tcp_listener.h"
#endif
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"
#ifdef USE_TCP_UART_MAX_CONNECTIONS
#include "esphome/core/helpers.h"
#endif

#include <cstdint>

namespace esphome::tcp_uart {

/// TCP client or server presented as a UART. Bytes are copied unchanged.
class TcpUart : public uart::UARTComponent, public Component {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(connected)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(disconnects)
#endif

 public:
  TcpUart() {
    this->rx_buffer_size_ = RX_BUFFER_SIZE;
#ifdef USE_TCP_UART_MAX_CONNECTIONS
    this->link_.set_tx_buffer(this->tx_);
#endif
  }

  void set_host(const char *host) { this->link_.set_host(host); }
  void set_port(uint16_t port) { this->link_.set_port(port); }
  void set_reconnect_interval(uint32_t ms) { this->link_.set_reconnect_interval(ms); }
  void set_idle_timeout(uint32_t ms) { this->link_.set_idle_timeout(ms); }
#ifdef USE_SOCKET_TCP_LISTENER
  void set_server(bool server) { this->server_ = server; }
#ifdef USE_SOCKET_IPV4_ALLOW
  void set_allow(const socket::Ipv4AllowEntry *entries, size_t count) { this->listener_.set_allow(entries, count); }
#endif
#endif
#ifdef USE_TCP_UART_MAX_CONNECTIONS
  /// Server: open this many connections at once. Call once, before setup().
  void set_max_connections(uint8_t count);
  void set_turn_timeout(uint16_t ms) { this->turn_timeout_ms_ = ms; }
#endif

  /// Changes whenever another connection, or a new session, gets the turn, and when that connection closes.
  /// Reads come from that connection only, writes go to it.
  uint16_t turn() const { return this->turn_; }
  /// Keeps a started turn on its connection until release_turn(). Held after a release, the turn reads nothing new.
  /// A held turn counts as traffic, so the idle timeout does not close a connection that waits for its reply.
  void hold_turn() {
    this->held_ = true;
#ifdef USE_TCP_UART_MAX_CONNECTIONS
    this->rest_ = 0;
#endif
  }
  /// Ends the turn as soon as the bytes already read from its connection are consumed and no hold is left.
  /// Before that it reads only the `rest` bytes missing from a request started in them, until turn_timeout of quiet.
  /// Pass `rest` no later than the loop() that takes the last available byte; each call replaces it.
  void release_turn(uint16_t rest = 0) {
    this->held_ = false;
#ifdef USE_TCP_UART_MAX_CONNECTIONS
    this->release_ = true;
    this->rest_ = rest;
#endif
  }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override { return static_cast<size_t>(this->rx_end_ - this->rx_start_); }
  // Same room write_array() grants, so consumers can apply backpressure.
  size_t available_for_write() override {
    Link *link = this->tx_link_();
    return link != nullptr ? link->tx_free() : 0;
  }
  uart::UARTFlushResult flush() override;
  bool is_connected() override;
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
  using UARTComponent::load_settings;  // also bring in the no-arg overload for convenience
#endif

 protected:
#ifdef USE_TCP_UART_MAX_CONNECTIONS
  // All connections share tx_; only the one that writes go to holds bytes in it.
  using Link = socket::TcpClientLinkSharedTx;
#else
  using Link = socket::TcpClientLink;
#endif

  void check_logger_conflict() override {}
  void sync_link_();
  /// Reads at most limit bytes and returns how many arrived.
  size_t read_socket_(Link &link, size_t limit = RX_BUFFER_SIZE);
  void publish_connected_(bool up);
  void count_disconnect_();
  /// The connection that writes go to, or nullptr.
  Link *tx_link_() {
#ifdef USE_TCP_UART_MAX_CONNECTIONS
    if (!this->extra_.empty()) {
      return this->tx_slot_ == NO_SLOT ? nullptr : &this->slot_(this->tx_slot_);
    }
#endif
    return &this->link_;
  }
#ifdef USE_TCP_UART_MAX_CONNECTIONS
  static constexpr uint8_t NO_SLOT = 0xFF;

  Link &slot_(uint8_t i) { return i == 0 ? this->link_ : this->extra_[i - 1]; }
  uint8_t slot_count_() const { return static_cast<uint8_t>(this->extra_.size() + 1); }
  void loop_pool_(bool waiting);
  void slot_edge_(uint8_t i);
  void accept_slot_(uint32_t now);
  void pick_holder_(uint32_t now);
  /// True once the last holder's reply has left the shared TX buffer. Closes that connection when it takes no byte
  /// for TX_STALL_TIMEOUT_MS while another client waits.
  bool tx_drained_(uint32_t now);
  void read_holder_(uint32_t now);
  void end_turn_(uint32_t now);
#endif

  static constexpr size_t RX_BUFFER_SIZE = 1024;

  Link link_;
#ifdef USE_SOCKET_TCP_LISTENER
  socket::TcpListener listener_;
#endif
#ifdef USE_SENSOR
  uint32_t disconnects_{0};
#endif
  uint32_t last_drop_log_ms_{0};
#ifdef USE_TCP_UART_MAX_CONNECTIONS
  // Slots 1 and up; slot 0 is link_, which also holds the port and the listener's retry clock.
  FixedVector<Link> extra_;
  // Last byte in either direction on the holder's connection.
  uint32_t last_traffic_ms_{0};
  // When the last holder's reply last left the TX buffer, and the free room seen then.
  uint32_t tx_moved_ms_{0};
  uint16_t tx_free_seen_{0};
  uint8_t holder_{NO_SLOT};
  // Writes go here: the holder, else the last holder until it closes.
  uint8_t tx_slot_{NO_SLOT};
  // The search for the next holder starts after this slot.
  uint8_t last_holder_{0};
  // Slots loop() saw connected.
  uint8_t up_mask_{0};
  // Slots whose last read stopped before EAGAIN; ready() stays false for them.
  uint8_t pending_mask_{0};
  bool release_{false};
  // Bytes the released turn may still read: the rest of a request started in the bytes already read.
  uint16_t rest_{0};
  uint16_t turn_timeout_ms_{100};
#endif
  // rx_[rx_start_, rx_end_) holds unread bytes; read_socket_() compacts to the front.
  uint16_t rx_start_{0};
  uint16_t rx_end_{0};
  uint16_t turn_{0};
  bool server_{false};
  // A consumer holds the turn until its reply.
  bool held_{false};
  // The link state loop() saw last; edges publish the sensor, the up edge clears rx_.
  bool link_was_up_{false};
  // A read stopped before EAGAIN. ready() stays false until new data arrives.
  bool rx_pending_{false};
  uint8_t rx_[RX_BUFFER_SIZE]{};
#ifdef USE_TCP_UART_MAX_CONNECTIONS
  uint8_t tx_[Link::TX_BUFFER_SIZE]{};
#endif
};

}  // namespace esphome::tcp_uart
