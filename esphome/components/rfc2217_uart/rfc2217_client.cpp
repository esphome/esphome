#include "esphome/core/defines.h"
#ifdef USE_RFC2217_UART_CLIENT

#include "rfc2217_uart.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome::rfc2217_uart {

ESPHOME_LOG_TAG(TAG, "rfc2217_uart");

// How long payload waits for the line settings after a connect; pySerial waits as long for their answers.
static constexpr uint32_t SETTINGS_WAIT_MS = 3000;

void Rfc2217Client::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RFC 2217 UART:\n"
                "  Role: client\n"
                "  Baud Rate: %" PRIu32 " baud\n"
                "  Data Bits: %u\n"
                "  Parity: %s\n"
                "  Stop Bits: %u",
                this->baud_rate_, this->data_bits_, LOG_STR_ARG(uart::parity_to_str(this->parity_)), this->stop_bits_);
}

void Rfc2217Client::on_link(bool up) {
  if (this->tx_len_ != 0) {
    this->note_drop_(LOG_STR("Link down, dropped the unsent bytes"));
    this->tx_len_ = 0;
  }
  // Unread bytes of the last session stay readable while down, never into the next one.
  if (up) {
    this->rx_.clear();
    this->settings_pending_ = true;
    this->link_up_ms_ = App.get_loop_component_start_time();
  }
}

bool Rfc2217Client::tx_held_() const {
  // Payload ahead of the settings would leave on the server's previous line. A server that refuses COM-PORT or does
  // not answer gets the payload anyway.
  const bool settings_due =
      this->us_[COM_PORT] == OptionState::WANT_YES || (this->com_port_() && this->settings_pending_);
  return settings_due && App.get_loop_component_start_time() - this->link_up_ms_ < SETTINGS_WAIT_MS;
}

void Rfc2217Client::send_settings_() {
  if (!this->is_connected() || !this->com_port_() || this->tcp_->available_for_write() < SETTINGS_SIZE) {
    return;
  }
  this->settings_pending_ = false;
  const uint8_t baud[4] = {
      static_cast<uint8_t>(this->baud_rate_ >> 24),
      static_cast<uint8_t>(this->baud_rate_ >> 16),
      static_cast<uint8_t>(this->baud_rate_ >> 8),
      static_cast<uint8_t>(this->baud_rate_),
  };
  this->send_command_(COM_SET_BAUDRATE, baud, sizeof(baud));
  this->send_command_(COM_SET_DATASIZE, &this->data_bits_, 1);
  const uint8_t parity = to_rfc_parity(this->parity_);
  this->send_command_(COM_SET_PARITY, &parity, 1);
  this->send_command_(COM_SET_STOPSIZE, &this->stop_bits_, 1);
}

void Rfc2217Client::check_answer_(const LogString *command, uint32_t asked, uint32_t got) {
  if (asked != got) {
    ESP_LOGW(TAG, "Server answered %s with %" PRIu32 ", asked for %" PRIu32, LOG_STR_ARG(command), got, asked);
  }
}

void Rfc2217Client::on_command(uint8_t code, const uint8_t *value, size_t len) {
  switch (code) {
    case SERVER_OFFSET + COM_SIGNATURE:
      // Without text it asks for ours.
      if (len == 0) {
        static constexpr uint8_t SIGNATURE[] = {'E', 'S', 'P', 'H', 'o', 'm', 'e'};
        this->send_command_(COM_SIGNATURE, SIGNATURE, sizeof(SIGNATURE));
      }
      return;
    case SERVER_OFFSET + COM_FLOWCONTROL_SUSPEND:
      this->peer_suspended_ = true;
      return;
    case SERVER_OFFSET + COM_FLOWCONTROL_RESUME:
      this->peer_suspended_ = false;
      return;
    case SERVER_OFFSET + COM_SET_BAUDRATE:
      if (len >= 4) {
        this->check_answer_(LOG_STR("SET-BAUDRATE"), this->baud_rate_,
                            encode_uint32(value[0], value[1], value[2], value[3]));
      }
      return;
    case SERVER_OFFSET + COM_SET_DATASIZE:
      if (len >= 1) {
        this->check_answer_(LOG_STR("SET-DATASIZE"), this->data_bits_, value[0]);
      }
      return;
    case SERVER_OFFSET + COM_SET_PARITY:
      if (len >= 1) {
        this->check_answer_(LOG_STR("SET-PARITY"), to_rfc_parity(this->parity_), value[0]);
      }
      return;
    case SERVER_OFFSET + COM_SET_STOPSIZE:
      if (len >= 1) {
        this->check_answer_(LOG_STR("SET-STOPSIZE"), this->stop_bits_, value[0]);
      }
      return;
    default:
      // Line and modem state notices and the other answers carry nothing this UART reports.
      return;
  }
}

size_t Rfc2217Client::payload_room() {
  const size_t room = RX_SIZE - this->available();
  // While the server has suspended, its RESUME may sit behind bytes nobody reads: like a UART overrun, the oldest go.
  if (room == 0 && this->peer_suspended_) {
    return READ_CHUNK;
  }
  return room;
}

void Rfc2217Client::deliver(const uint8_t *data, size_t len) {
  const size_t room = RX_SIZE - this->available();
  if (len > room) {
    this->note_drop_(LOG_STR("RX buffer full, dropped the oldest bytes"));
    for (size_t i = len - room; i != 0; i--) {
      this->rx_.pop();
    }
  }
  this->inject_rx(data, len);
}

void Rfc2217Client::write_array(const uint8_t *data, size_t len) {
  this->debug_tx_(data, len);
  if (!this->is_connected()) {
    this->note_drop_(LOG_STR("Not connected, dropped"));
    return;
  }
  while (len != 0) {
    const size_t take = std::min(len, TX_SIZE - this->tx_len_);
    if (take == 0) {
      this->note_drop_(LOG_STR("TX buffer full, dropped"));
      return;
    }
    std::memcpy(this->tx_ + this->tx_len_, data, take);
    this->tx_len_ = static_cast<uint16_t>(this->tx_len_ + take);
    data += take;
    len -= take;
    this->send_tx_();
  }
}

void Rfc2217Client::send_tx_() {
  if (this->tx_len_ == 0 || this->peer_suspended_ || !this->is_connected() || this->tx_held_()) {
    return;
  }
  uint8_t out[TX_SIZE];
  size_t used = 0;
  const size_t n =
      telnet_escape(this->tx_, this->tx_len_, out, std::min(this->tcp_->available_for_write(), sizeof(out)), &used);
  if (n == 0) {
    return;
  }
  this->write_tcp_(out, n);
  this->tx_len_ = static_cast<uint16_t>(this->tx_len_ - used);
  std::memmove(this->tx_, this->tx_ + used, this->tx_len_);
}

size_t Rfc2217Client::available_for_write() { return this->is_connected() ? TX_SIZE - this->tx_len_ : 0; }

uart::UARTFlushResult Rfc2217Client::flush() {
  this->send_tx_();
  if (!this->is_connected()) {
    return uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  }
  this->wrote_ = false;
  // Held for the line settings, while the server has suspended, or while the link has no room.
  if (this->tx_len_ != 0) {
    this->tcp_->flush();
    return uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
  }
  return this->tcp_->flush();
}

}  // namespace esphome::rfc2217_uart

#endif  // USE_RFC2217_UART_CLIENT
