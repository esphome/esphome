#pragma once

#include "telnet.h"
#include "esphome/components/tcp_uart/tcp_uart.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_ESP32
#include "esphome/components/uart/uart_component_esp_idf.h"
#endif
#ifdef USE_RFC2217_UART_CLIENT
#include "esphome/components/uart/uart_virtual.h"
#endif
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include <cstddef>
#include <cstdint>

namespace esphome::rfc2217_uart {

/// [RFC 2217] SET-PARITY value of a UART parity.
inline uint8_t to_rfc_parity(uart::UARTParityOptions parity) {
  switch (parity) {
    case uart::UART_CONFIG_PARITY_ODD:
      return PARITY_ODD;
    case uart::UART_CONFIG_PARITY_EVEN:
      return PARITY_EVEN;
    default:
      return PARITY_NONE;
  }
}

/// Telnet with the COM-PORT option on a tcp_uart: option negotiation, escaping and RFC 2217 flow control.
class Rfc2217Base : public Component {
 public:
  void set_tcp_uart(tcp_uart::TcpUart *tcp) { this->tcp_ = tcp; }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  /// [RFC 1143] option state, without the queue.
  enum class OptionState : uint8_t {
    OPTION_STATE_NO,
    OPTION_STATE_YES,
    OPTION_STATE_WANT_YES,
  };
  static constexpr size_t BINARY = 0;
  static constexpr size_t COM_PORT = 1;
  static constexpr size_t READ_CHUNK = 128;

  /// Starts or ends a session when the link state changed.
  void link_edge_();
  /// Takes received bytes while the payload has room. Payload goes to deliver(), COM-PORT commands to on_command().
  void read_tcp_();
  void write_tcp_(const uint8_t *data, size_t len);
  void flush_tcp_();
  /// Sends a COM-PORT command; the server's codes get SERVER_OFFSET. False without room.
  bool send_command_(uint8_t code, const uint8_t *value, size_t len);
  /// Answers a SIGNATURE request with this side's text.
  void send_signature_();
  /// COM-PORT is enabled in the direction RFC 2217 uses: client WILL, server DO.
  bool com_port_() const {
    return (this->server_ ? this->him_[COM_PORT] : this->us_[COM_PORT]) == OptionState::OPTION_STATE_YES;
  }
  /// The payload buffer the peer fills crossed 3/4 or 1/4.
  bool flow_due_(size_t level, size_t capacity) const {
    return this->com_port_() && (this->suspended_peer_ ? level <= capacity / 4 : level >= capacity * 3 / 4);
  }
  void update_flow_();
  void on_option_(uint8_t verb, uint8_t option);
  void send_option_(uint8_t verb, uint8_t option);
  void read_plain_();

  virtual size_t payload_room() = 0;
  virtual void deliver(const uint8_t *data, size_t len) = 0;
  virtual void on_command(uint8_t code, const uint8_t *value, size_t len) = 0;
  virtual void on_link(bool up) = 0;

  tcp_uart::TcpUart *tcp_{nullptr};
  TelnetDecoder decoder_;
  // us_ is this side's option, him_ the peer's.
  OptionState us_[2]{};
  OptionState him_[2]{};
  bool server_{false};
  bool link_was_up_{false};
  bool wrote_{false};
  // The peer sent FLOWCONTROL-SUSPEND: payload for it waits. Answers still go; they are short and only follow a
  // command, so a suspend on both sides cannot block them.
  bool peer_suspended_{false};
  // This side sent FLOWCONTROL-SUSPEND.
  bool suspended_peer_{false};
};

/// RFC 2217 access server for a hardware UART.
class Rfc2217Server : public Rfc2217Base, public uart::UARTDevice {
 public:
  Rfc2217Server() { this->server_ = true; }
#ifdef USE_ESP32
  /// The UART is an IDFUARTComponent.
  void set_idf_uart(bool idf_uart) { this->idf_uart_ = idf_uart; }
#endif

  void setup() override;
  void loop() override {
    if (this->tcp_->is_connected() != this->link_was_up_) {
      this->link_edge_();
    }
    if (!this->link_was_up_) {
      if (this->ending_) {
        this->end_session_();
      }
      return;
    }
    if (this->tcp_->available() != 0) {
      this->read_link_();
    }
    if (this->to_serial_len_ != 0) {
      this->write_serial_();
    }
    if (this->answering_()) {
      this->apply_line_();
    }
    if (this->flow_due_(this->to_serial_len_, TO_SERIAL_SIZE)) {
      this->update_flow_();
    }
    if (!this->peer_suspended_ && this->available() != 0) {
      this->read_serial_();
    }
    if (this->wrote_) {
      this->flush_tcp_();
    }
  }
  void dump_config() override;

 protected:
  struct Line {
    uint32_t baud_rate;
    uart::UARTParityOptions parity;
    uint8_t data_bits;
    uint8_t stop_bits;
    bool operator==(const Line &other) const {
      return this->baud_rate == other.baud_rate && this->data_bits == other.data_bits && this->parity == other.parity &&
             this->stop_bits == other.stop_bits;
    }
  };

  /// Loads the line the setters hold into the UART; false where it cannot change at runtime.
  virtual bool reload_serial();
  Line line_() const;
  /// Sets the line and reloads the UART; on failure the setters get the old line back.
  void set_line_(const Line &line);
  /// The line the SET-* commands of the current batch build on.
  Line &pending_();
  bool answering_() const {
    return (this->answers_due_[0] | this->answers_due_[1] | this->answers_due_[2] | this->answers_due_[3]) != 0;
  }
  /// Applies the SET-* commands that arrived together in one reload and answers each with the value in use.
  void apply_line_();
  void read_link_();
  /// After the peer closed: writes the rest of its payload, then puts the configured line back.
  void end_session_();
  bool answer_(uint8_t code, uint8_t value);
  /// Nothing waits in the UART's TX FIFO, which a reload empties.
  bool tx_idle_();
  void write_serial_();
  void read_serial_();
  void discard_serial_();
  size_t payload_room() override { return TO_SERIAL_SIZE - this->to_serial_len_; }
  void deliver(const uint8_t *data, size_t len) override;
  void on_command(uint8_t code, const uint8_t *value, size_t len) override;
  void on_link(bool up) override;

  static constexpr size_t TO_SERIAL_SIZE = 256;
  static constexpr size_t DISCARD_CHUNK = 32;

  // Loop start time of the last write to the UART; sizes the next paced write.
  uint32_t last_write_ms_{0};
  // The line from the YAML, back in use when a session ends.
  Line configured_{};
  Line pending_line_{};
  // to_serial_[0, to_serial_len_): payload from the client that waits for the UART.
  uint16_t to_serial_len_{0};
  // While answers are due: the payload in to_serial_ that came before the SET-* commands, for the old line.
  uint16_t fence_{0};
  // [n]: answers due to SET-* command n + 1.
  uint8_t answers_due_[4]{};
  // The peer closed and the configured line is not back yet.
  bool ending_{false};
#ifdef USE_ESP32
  bool idf_uart_{false};
#endif
  uint8_t to_serial_[TO_SERIAL_SIZE]{};
};

#ifdef USE_RFC2217_UART_CLIENT
/// RFC 2217 client: a UART whose line settings go to the access server.
class Rfc2217Client : public uart::VirtualUARTComponent, public Rfc2217Base {
 public:
  Rfc2217Client() : VirtualUARTComponent(RX_SIZE) {}

  void loop() override {
    if (this->tcp_->is_connected() != this->link_was_up_) {
      this->link_edge_();
    }
    if (this->tcp_->available() != 0) {
      // After the server closed, too: what it sent before stays readable.
      this->read_tcp_();
    }
    if (!this->link_was_up_) {
      return;
    }
    if (this->settings_pending_) {
      this->send_settings_();
    }
    if (this->tx_len_ != 0) {
      this->send_tx_();
    }
    if (this->wrote_) {
      this->flush_tcp_();
    }
  }
  void dump_config() override;

  void write_array(const uint8_t *data, size_t len) override;
  size_t available_for_write() override;
  uart::UARTFlushResult flush() override;
  // From this component's up edge on, so nothing is written ahead of the option offers.
  bool is_connected() override { return this->link_was_up_ && this->tcp_->is_connected(); }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  using UARTComponent::load_settings;
  // Sends the line settings to the server.
  void load_settings(bool dump_config) override { this->request_settings_(); }
#endif

 protected:
  /// The bytes written so far leave on the old line, the later ones after the new settings.
  void request_settings_();
  void send_settings_();
  void send_tx_();
  /// After a connect, payload waits for the line settings, which go out once the server accepts COM-PORT.
  bool tx_held_() const;
  void check_answer_(const LogString *command, uint32_t asked, uint32_t got);
  void note_drop_(const LogString *message);
  size_t payload_room() override;
  void deliver(const uint8_t *data, size_t len) override;
  void on_command(uint8_t code, const uint8_t *value, size_t len) override;
  void on_link(bool up) override;

  static constexpr uint16_t RX_SIZE = 256;
  static constexpr size_t TX_SIZE = 256;
  // Four commands: the baud rate with every byte doubled, three single bytes.
  static constexpr size_t SETTINGS_SIZE = COM_PORT_COMMAND_MAX + 3 * 7;

  // Loop start time of the up edge.
  uint32_t link_up_ms_{0};
  // One rate limit for all drop warnings.
  uint32_t drop_log_ms_{0};
  // tx_[0, tx_len_): written, not sent; held while the line settings are due or the server has suspended.
  uint16_t tx_len_{0};
  // While the settings are due: the bytes in tx_ written before them.
  uint16_t settings_fence_{0};
  bool settings_pending_{false};
  uint8_t tx_[TX_SIZE]{};
};
#endif  // USE_RFC2217_UART_CLIENT

}  // namespace esphome::rfc2217_uart
