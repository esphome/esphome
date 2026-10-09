#include "rfc2217_uart.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#ifdef USE_ESP32
#include <soc/soc_caps.h>
#endif

namespace esphome::rfc2217_uart {

ESPHOME_LOG_TAG(TAG, "rfc2217_uart");

// The ceiling the CDC-ACM bridge checks too; below it the driver decides.
#ifdef USE_ESP32
static constexpr uint32_t MAX_BAUD_RATE = SOC_UART_BITRATE_MAX;
#else
static constexpr uint32_t MAX_BAUD_RATE = 5000000;
#endif

void Rfc2217Server::setup() { this->configured_ = this->line_(); }

void Rfc2217Server::dump_config() {
  ESP_LOGCONFIG(TAG, "RFC 2217 UART:\n"
                     "  Role: server");
}

void Rfc2217Server::on_link(bool up) {
  if (!up) {
    // The payload the client sent before it closed still goes out; end_session_() then restores the line.
    this->ending_ = this->configured_.baud_rate != 0;
    return;
  }
  // A new session drops what the last one left.
  this->to_serial_len_ = 0;
  this->fence_ = 0;
  std::memset(this->answers_due_, 0, sizeof(this->answers_due_));
  if (this->ending_) {
    this->ending_ = false;
    this->set_line_(this->configured_);
  }
  // The driver kept whatever arrived while the link was down.
  this->discard_serial_();
}

void Rfc2217Server::read_link_() {
  // Commands after the payload that follows a batch wait in the link until the batch is answered.
  if (this->answering_() && this->to_serial_len_ != this->fence_) {
    this->read_plain_();
  } else {
    this->read_tcp_();
  }
}

void Rfc2217Server::end_session_() {
  if (this->tcp_->available() != 0) {
    this->read_link_();
  }
  if (this->to_serial_len_ != 0) {
    this->write_serial_();
  }
  if (this->answering_()) {
    this->apply_line_();
  }
  if (this->tcp_->available() != 0 || this->to_serial_len_ != 0 || this->answering_() || !this->tx_idle_()) {
    return;
  }
  // [RFC 2217] A new session starts on the configured line, not on the last client's.
  this->ending_ = false;
  this->set_line_(this->configured_);
}

void Rfc2217Server::deliver(const uint8_t *data, size_t len) {
  // read_tcp_() takes no more than payload_room().
  std::memcpy(this->to_serial_ + this->to_serial_len_, data, len);
  this->to_serial_len_ = static_cast<uint16_t>(this->to_serial_len_ + len);
}

void Rfc2217Server::write_serial_() {
  const size_t limit = this->answering_() ? this->fence_ : this->to_serial_len_;
  const size_t n = std::min(this->parent_->paced_write_room(this->last_write_ms_), limit);
  if (n == 0) {
    return;
  }
  this->write_array(this->to_serial_, n);
  this->last_write_ms_ = App.get_loop_component_start_time();
  this->to_serial_len_ = static_cast<uint16_t>(this->to_serial_len_ - n);
  std::memmove(this->to_serial_, this->to_serial_ + n, this->to_serial_len_);
  if (this->answering_()) {
    this->fence_ = static_cast<uint16_t>(this->fence_ - n);
  }
}

void Rfc2217Server::read_serial_() {
  uint8_t raw[READ_CHUNK];
  uint8_t out[2 * READ_CHUNK];
  while (true) {
    // Every byte may double.
    const size_t want = std::min({this->available(), this->tcp_->available_for_write() / 2, READ_CHUNK});
    if (want == 0 || !this->read_array(raw, want)) {
      return;
    }
    size_t used = 0;
    this->write_tcp_(out, telnet_escape(raw, want, out, sizeof(out), &used));
  }
}

void Rfc2217Server::discard_serial_() {
  // Drain exactly what is buffered; later bytes are live.
  uint8_t dump[DISCARD_CHUNK];
  size_t left = this->available();
  while (left != 0) {
    const size_t n = std::min(left, sizeof(dump));
    if (!this->read_array(dump, n)) {
      return;
    }
    left -= n;
  }
}

Rfc2217Server::Line Rfc2217Server::line_() const {
  const uart::UARTComponent *serial = this->parent_;
  return {serial->get_baud_rate(), serial->get_parity(), serial->get_data_bits(), serial->get_stop_bits()};
}

bool Rfc2217Server::reload_serial() {
#ifdef USE_ESP32
  if (this->idf_uart_) {
    // Keeps the driver; a rate it cannot reach leaves the previous line in place.
    return static_cast<uart::IDFUARTComponent *>(this->parent_)->apply_settings_live() == ESP_OK;
  }
  this->parent_->load_settings(false);
  return true;
#else
  // Off ESP32 the line stays as configured; ESP8266's reload does not wait for the bytes in flight.
  return false;
#endif
}

bool Rfc2217Server::tx_idle_() {
#ifdef USE_ESP32
  if (this->idf_uart_) {
    auto *serial = static_cast<uart::IDFUARTComponent *>(this->parent_);
    return uart_wait_tx_done(static_cast<uart_port_t>(serial->get_hw_serial_number()), 0) == ESP_OK;
  }
#endif
  return true;
}

void Rfc2217Server::set_line_(const Line &line) {
  const Line old = this->line_();
  if (line == old) {
    return;
  }
  uart::UARTComponent *serial = this->parent_;
  serial->set_baud_rate(line.baud_rate);
  serial->set_data_bits(line.data_bits);
  serial->set_parity(line.parity);
  serial->set_stop_bits(line.stop_bits);
  if (this->reload_serial()) {
    ESP_LOGD(TAG, "Line set to %" PRIu32 " baud, %u data bits, parity %s, %u stop bits", serial->get_baud_rate(),
             serial->get_data_bits(), LOG_STR_ARG(uart::parity_to_str(serial->get_parity())), serial->get_stop_bits());
    return;
  }
  // The answers then report the line that stays in use.
  serial->set_baud_rate(old.baud_rate);
  serial->set_data_bits(old.data_bits);
  serial->set_parity(old.parity);
  serial->set_stop_bits(old.stop_bits);
}

Rfc2217Server::Line &Rfc2217Server::pending_() {
  if (!this->answering_()) {
    this->pending_line_ = this->line_();
    this->fence_ = this->to_serial_len_;
  }
  return this->pending_line_;
}

void Rfc2217Server::apply_line_() {
  // The payload before the commands leaves on the old line first; a reload empties the TX FIFO.
  if (this->fence_ != 0 || !this->tx_idle_()) {
    return;
  }
  this->set_line_(this->pending_line_);
  if (!this->link_was_up_) {
    std::memset(this->answers_due_, 0, sizeof(this->answers_due_));
    return;
  }
  // Answers that find no room stay due for the next pass.
  const Line now = this->line_();
  const uint8_t baud[4] = {static_cast<uint8_t>(now.baud_rate >> 24), static_cast<uint8_t>(now.baud_rate >> 16),
                           static_cast<uint8_t>(now.baud_rate >> 8), static_cast<uint8_t>(now.baud_rate)};
  const uint8_t values[4] = {0, now.data_bits, to_rfc_parity(now.parity), now.stop_bits};
  for (uint8_t i = 0; i < 4; i++) {
    const uint8_t code = COM_SET_BAUDRATE + i;
    while (this->answers_due_[i] != 0 &&
           (i == 0 ? this->send_command_(code, baud, sizeof(baud)) : this->answer_(code, values[i]))) {
      this->answers_due_[i]--;
    }
  }
}

bool Rfc2217Server::answer_(uint8_t code, uint8_t value) { return this->send_command_(code, &value, 1); }

void Rfc2217Server::on_command(uint8_t code, const uint8_t *value, size_t len) {
  // [RFC 2217] Every command is answered with the value in use, which may differ from the one asked for.
  switch (code) {
    case COM_FLOWCONTROL_SUSPEND:
      this->peer_suspended_ = true;
      return;
    case COM_FLOWCONTROL_RESUME:
      this->peer_suspended_ = false;
      return;
    case COM_SIGNATURE:
      // Without text it asks for ours.
      if (len == 0) {
        this->send_signature_();
      }
      return;
    default:
      break;
  }
  if (len == 0) {
    return;
  }
  const uint8_t v = value[0];
  switch (code) {
    case COM_SET_BAUDRATE: {
      if (len < 4) {
        return;
      }
      const uint32_t asked = encode_uint32(value[0], value[1], value[2], value[3]);
      Line &line = this->pending_();
      // 0 asks for the value in use.
      if (asked != 0 && asked <= MAX_BAUD_RATE) {
        line.baud_rate = asked;
      }
      break;
    }
    case COM_SET_DATASIZE: {
      Line &line = this->pending_();
      if (v >= 5 && v <= 8) {
        line.data_bits = v;
      }
      break;
    }
    case COM_SET_PARITY: {
      Line &line = this->pending_();
      // MARK and SPACE have no UARTParityOptions value.
      if (v == PARITY_NONE) {
        line.parity = uart::UART_CONFIG_PARITY_NONE;
      } else if (v == PARITY_ODD) {
        line.parity = uart::UART_CONFIG_PARITY_ODD;
      } else if (v == PARITY_EVEN) {
        line.parity = uart::UART_CONFIG_PARITY_EVEN;
      }
      break;
    }
    case COM_SET_STOPSIZE: {
      Line &line = this->pending_();
      // 3 is 1.5 stop bits.
      if (v == 1 || v == 2) {
        line.stop_bits = v;
      }
      break;
    }
    case COM_SET_CONTROL:
      // The UART has no flow control, BREAK, DTR or RTS: each is answered off.
      if (v <= 3 || v == 17 || v == 19) {
        this->answer_(COM_SET_CONTROL, CONTROL_NO_FLOW);
      } else if (v <= 6) {
        this->answer_(COM_SET_CONTROL, CONTROL_BREAK_OFF);
      } else if (v <= 9) {
        this->answer_(COM_SET_CONTROL, CONTROL_DTR_OFF);
      } else if (v <= 12) {
        this->answer_(COM_SET_CONTROL, CONTROL_RTS_OFF);
      } else if (v <= 16 || v == 18) {
        this->answer_(COM_SET_CONTROL, CONTROL_NO_FLOW_IN);
      }
      return;
    case COM_SET_LINESTATE_MASK:
    case COM_SET_MODEMSTATE_MASK:
      // No line or modem state is ever reported.
      this->answer_(code, 0);
      return;
    case COM_PURGE_DATA:
      if (v < 1 || v > 3) {
        return;
      }
      // 1 purges the bytes from the UART for the client, 2 those for the UART not yet written.
      if (v & 1) {
        this->discard_serial_();
      }
      if (v & 2) {
        this->to_serial_len_ = 0;
        this->fence_ = 0;
      }
      this->answer_(COM_PURGE_DATA, v);
      return;
    default:
      return;
  }
  // A SET-* command: answered once the commands that arrived with it are applied.
  uint8_t &due = this->answers_due_[code - COM_SET_BAUDRATE];
  if (due != UINT8_MAX) {
    due++;
  }
}

}  // namespace esphome::rfc2217_uart
