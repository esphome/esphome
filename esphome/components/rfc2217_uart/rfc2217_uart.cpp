#include "rfc2217_uart.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome::rfc2217_uart {

ESPHOME_LOG_TAG(TAG, "rfc2217_uart");

static constexpr uint32_t DROP_LOG_INTERVAL_MS = 5000;

void Rfc2217Base::note_drop_(const LogString *message) {
  uint32_t now = App.get_loop_component_start_time();
  if (this->drop_log_ms_ != 0 && now - this->drop_log_ms_ < DROP_LOG_INTERVAL_MS) {
    return;
  }
  // A zero stamp would look like "never logged" on the next pass.
  this->drop_log_ms_ = now == 0 ? 1 : now;
  ESP_LOGW(TAG, "%s", LOG_STR_ARG(message));
}

void Rfc2217Base::link_edge_() {
  const bool up = this->tcp_->is_connected();
  this->link_was_up_ = up;
  this->decoder_.reset();
  for (size_t i = 0; i < 2; i++) {
    this->us_[i] = OptionState::NO;
    this->him_[i] = OptionState::NO;
  }
  this->peer_suspended_ = false;
  this->suspended_peer_ = false;
  this->on_link(up);
  if (!up) {
    return;
  }
  this->us_[BINARY] = OptionState::WANT_YES;
  this->him_[BINARY] = OptionState::WANT_YES;
  this->send_option_(TELNET_WILL, OPTION_BINARY);
  this->send_option_(TELNET_DO, OPTION_BINARY);
  // [RFC 2217] The client offers COM-PORT and the server accepts it; either may start.
  if (this->server_) {
    this->him_[COM_PORT] = OptionState::WANT_YES;
    this->send_option_(TELNET_DO, OPTION_COM_PORT);
  } else {
    this->us_[COM_PORT] = OptionState::WANT_YES;
    this->send_option_(TELNET_WILL, OPTION_COM_PORT);
  }
}

void Rfc2217Base::write_tcp_(const uint8_t *data, size_t len) {
  this->tcp_->write_array(data, len);
  this->wrote_ = true;
}

void Rfc2217Base::flush_tcp_() {
  // Send now, not on tcp_uart's next pass.
  this->wrote_ = false;
  this->tcp_->flush();
}

void Rfc2217Base::send_option_(uint8_t verb, uint8_t option) {
  const uint8_t command[3] = {TELNET_IAC, verb, option};
  this->write_tcp_(command, sizeof(command));
}

bool Rfc2217Base::send_command_(uint8_t code, const uint8_t *value, size_t len) {
  uint8_t command[COM_PORT_COMMAND_MAX];
  size_t n = write_com_port(command, this->server_ ? code + SERVER_OFFSET : code, value, len);
  if (this->tcp_->available_for_write() < n) {
    return false;
  }
  this->write_tcp_(command, n);
  return true;
}

void Rfc2217Base::on_option_(uint8_t verb, uint8_t option) {
  const bool positive = verb == TELNET_WILL || verb == TELNET_DO;
  // WILL and WONT are about the peer's side of the option, DO and DONT about this side.
  const bool his = verb == TELNET_WILL || verb == TELNET_WONT;
  const uint8_t yes = his ? TELNET_DO : TELNET_WILL;
  const uint8_t no = his ? TELNET_DONT : TELNET_WONT;
  size_t index;
  if (option == OPTION_BINARY) {
    index = BINARY;
  } else if (option == OPTION_COM_PORT) {
    index = COM_PORT;
  } else {
    // Refuse every other option; a refusal needs no answer.
    if (positive) {
      this->send_option_(no, option);
    }
    return;
  }
  OptionState &state = his ? this->him_[index] : this->us_[index];
  const bool asked = state == OptionState::WANT_YES;
  if (positive) {
    if (state == OptionState::NO) {
      this->send_option_(yes, option);
    }
    state = OptionState::YES;
    return;
  }
  if (state == OptionState::YES) {
    this->send_option_(no, option);
  }
  state = OptionState::NO;
  if (!asked) {
    return;
  }
  if (index == BINARY) {
    // [RFC 854] Without BINARY the peer may add NUL after CR and drop the top bit.
    ESP_LOGW(TAG, "Peer refused BINARY, the payload may be changed");
    return;
  }
  // The side RFC 2217 needs: the client's WILL, the server's DO.
  if (his == this->server_) {
    ESP_LOGW(TAG, "Peer refused the COM-PORT option, the line settings are not exchanged");
  }
}

void Rfc2217Base::read_plain_() {
  // No room for an answer: take payload up to the next command, which waits in the link.
  uint8_t payload[READ_CHUNK];
  size_t n = 0;
  size_t room = std::min(this->payload_room(), sizeof(payload));
  uint8_t byte;
  while (n < room && this->decoder_.idle() && this->tcp_->peek_byte(&byte) && byte != TELNET_IAC) {
    this->tcp_->read_array(&byte, 1);
    payload[n++] = byte;
  }
  if (n != 0) {
    this->deliver(payload, n);
  }
}

void Rfc2217Base::read_tcp_() {
  // Answers take at most 13 bytes per 6 bytes read (SIGNATURE), plus one command begun in an earlier read.
  const size_t tx_room = this->tcp_->available_for_write();
  if (tx_room < COM_PORT_COMMAND_MAX + 3) {
    this->read_plain_();
    return;
  }
  const size_t n =
      std::min({this->tcp_->available(), this->payload_room(), READ_CHUNK, (tx_room - COM_PORT_COMMAND_MAX) / 3});
  uint8_t raw[READ_CHUNK];
  if (n == 0 || !this->tcp_->read_array(raw, n)) {
    return;
  }
  // Payload never outgrows the read, and n fits payload_room().
  uint8_t payload[READ_CHUNK];
  size_t len = 0;
  size_t i = 0;
  while (i < n) {
    if (this->decoder_.idle()) {
      const auto *iac = static_cast<const uint8_t *>(std::memchr(raw + i, TELNET_IAC, n - i));
      const size_t run = static_cast<size_t>((iac == nullptr ? raw + n : iac) - (raw + i));
      std::memcpy(payload + len, raw + i, run);
      len += run;
      i += run;
      if (i == n) {
        break;
      }
    }
    switch (this->decoder_.feed(raw[i++])) {
      case TelnetDecoder::Event::DATA:
        payload[len++] = this->decoder_.data();
        break;
      case TelnetDecoder::Event::OPTION:
        this->on_option_(this->decoder_.verb(), this->decoder_.option());
        break;
      case TelnetDecoder::Event::SUBNEGOTIATION: {
        // A command such as PURGE-DATA applies to the payload before it, so that goes first.
        if (len != 0) {
          this->deliver(payload, len);
          len = 0;
        }
        const uint8_t *sub = this->decoder_.sub();
        const size_t sub_len = this->decoder_.sub_len();
        if (sub_len >= 2 && sub[0] == OPTION_COM_PORT) {
          this->on_command(sub[1], sub + 2, sub_len - 2);
        }
        break;
      }
      case TelnetDecoder::Event::NONE:
        break;
    }
  }
  if (len != 0) {
    this->deliver(payload, len);
  }
}

void Rfc2217Base::update_flow_() {
  if (!this->suspended_peer_) {
    this->suspended_peer_ = this->send_command_(COM_FLOWCONTROL_SUSPEND, nullptr, 0);
  } else {
    this->suspended_peer_ = !this->send_command_(COM_FLOWCONTROL_RESUME, nullptr, 0);
  }
}

}  // namespace esphome::rfc2217_uart
