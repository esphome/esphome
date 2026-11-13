#include "rfc2217_uart.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome::rfc2217_uart {

ESPHOME_LOG_TAG(TAG, "rfc2217_uart");

void Rfc2217Base::link_edge_() {
  const bool up = this->tcp_->is_connected();
  this->link_was_up_ = up;
  this->peer_suspended_ = false;
  this->suspended_peer_ = false;
  // The decoder and the options stay for the bytes the peer sent before it closed.
  if (up) {
    this->decoder_.reset();
    this->us_[BINARY] = OptionState::OPTION_STATE_WANT_YES;
    this->him_[BINARY] = OptionState::OPTION_STATE_WANT_YES;
    // [RFC 2217] The client offers COM-PORT and the server accepts it; either may start.
    this->us_[COM_PORT] = this->server_ ? OptionState::OPTION_STATE_NO : OptionState::OPTION_STATE_WANT_YES;
    this->him_[COM_PORT] = this->server_ ? OptionState::OPTION_STATE_WANT_YES : OptionState::OPTION_STATE_NO;
  }
  this->on_link(up);
  if (!up) {
    return;
  }
  this->send_option_(TELNET_WILL, OPTION_BINARY);
  this->send_option_(TELNET_DO, OPTION_BINARY);
  this->send_option_(this->server_ ? TELNET_DO : TELNET_WILL, OPTION_COM_PORT);
}

void Rfc2217Base::write_tcp_(const uint8_t *data, size_t len) {
  // Answers to the bytes read after the peer closed have nowhere to go.
  if (!this->link_was_up_) {
    return;
  }
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
  const bool asked = state == OptionState::OPTION_STATE_WANT_YES;
  const bool was_yes = state == OptionState::OPTION_STATE_YES;
  if (positive) {
    if (state == OptionState::OPTION_STATE_NO) {
      this->send_option_(yes, option);
    }
    state = OptionState::OPTION_STATE_YES;
    return;
  }
  if (was_yes) {
    this->send_option_(no, option);
  }
  state = OptionState::OPTION_STATE_NO;
  if (!asked && !was_yes) {
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
  const size_t room = std::min(this->payload_room(), sizeof(payload));
  uint8_t byte;
  while (n < room && this->tcp_->peek_byte(&byte)) {
    // After an IAC only a second one, an escaped 0xFF, is payload.
    if (!this->decoder_.idle() && (!this->decoder_.at_iac() || byte != TELNET_IAC)) {
      break;
    }
    this->tcp_->read_array(&byte, 1);
    if (this->decoder_.feed(byte) == TelnetDecoder::Event::EVENT_DATA) {
      payload[n++] = this->decoder_.data();
    }
  }
  if (n != 0) {
    this->deliver(payload, n);
  }
}

void Rfc2217Base::read_tcp_() {
  // Answers take at most 13 bytes per 6 bytes read (SIGNATURE), plus one command begun in an earlier read.
  // After the peer closed, nothing is answered.
  const size_t tx_room = this->link_was_up_ ? this->tcp_->available_for_write() : SIZE_MAX;
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
      case TelnetDecoder::Event::EVENT_DATA:
        payload[len++] = this->decoder_.data();
        break;
      case TelnetDecoder::Event::EVENT_OPTION:
        this->on_option_(this->decoder_.verb(), this->decoder_.option());
        break;
      case TelnetDecoder::Event::EVENT_SUBNEGOTIATION: {
        // A command such as PURGE-DATA applies to the payload before it, so that goes first.
        if (len != 0) {
          this->deliver(payload, len);
          len = 0;
        }
        const uint8_t *sub = this->decoder_.sub();
        const size_t sub_len = this->decoder_.sub_len();
        // Not gated on the option: pySerial sends no WILL COM-PORT when the server's DO arrives first.
        if (sub_len >= 2 && sub[0] == OPTION_COM_PORT) {
          this->on_command(sub[1], sub + 2, sub_len - 2);
        }
        break;
      }
      case TelnetDecoder::Event::EVENT_NONE:
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
