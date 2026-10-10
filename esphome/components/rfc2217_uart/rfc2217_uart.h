#pragma once

#include "telnet.h"
#include "esphome/components/tcp_uart/tcp_uart.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_ESP32
#include "esphome/components/uart/uart_component_esp_idf.h"
#endif
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include <cstddef>
#include <cstdint>

namespace esphome::rfc2217_uart {

/// [RFC 2217] SET-PARITY value of a UART parity.
inline uint8_t to_rfc_parity(uart::UARTParityOptions parity) {
  return parity == uart::UART_CONFIG_PARITY_ODD    ? PARITY_ODD
         : parity == uart::UART_CONFIG_PARITY_EVEN ? PARITY_EVEN
                                                   : PARITY_NONE;
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
  static constexpr uint8_t SIGNATURE[] = {'E', 'S', 'P', 'H', 'o', 'm', 'e'};

  /// Starts or ends a session when the link state changed.
  void link_edge_();
  /// Takes received bytes while the payload has room. Payload goes to deliver(), COM-PORT commands to on_command().
  void read_tcp_();
  void write_tcp_(const uint8_t *data, size_t len);
  void flush_tcp_();
  /// Sends a COM-PORT command; the server's codes get SERVER_OFFSET. False without room.
  bool send_command_(uint8_t code, const uint8_t *value, size_t len);
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
    if (!this->link_was_up_ && !this->ending_) {
      return;
    }
    // Commands after the payload that follows a batch wait in the link until the batch is answered. Commands in the
    // same TCP read join the batch; a client that waits for each answer, as pySerial does, is not affected.
    if (this->answering_() && this->to_serial_len_ != this->fence_) {
      this->read_plain_();
    } else if (this->tcp_->available() != 0) {
      this->read_tcp_();
    }
    if (this->to_serial_len_ != 0) {
      this->write_serial_();
    }
    if (this->answering_()) {
      this->apply_line_();
    }
    if (!this->link_was_up_) {
      // [RFC 2217] A new session starts on the configured line, not on the last client's.
      if (this->tcp_->available() == 0 && this->to_serial_len_ == 0 && !this->answering_() && this->tx_idle_()) {
        this->ending_ = false;
        this->set_line_(this->configured_);
      }
      return;
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
    bool operator==(const Line &other) const = default;
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
  bool answer_(uint8_t code, uint8_t value) { return this->send_command_(code, &value, 1); }
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

}  // namespace esphome::rfc2217_uart
