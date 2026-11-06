#include "telnet.h"

namespace esphome::rfc2217_uart {

TelnetDecoder::Event TelnetDecoder::after_iac_(uint8_t byte) {
  switch (byte) {
    case TELNET_IAC:
      this->state_ = State::STATE_DATA;
      this->data_ = TELNET_IAC;
      return Event::EVENT_DATA;
    case TELNET_WILL:
    case TELNET_WONT:
    case TELNET_DO:
    case TELNET_DONT:
      this->state_ = State::STATE_OPTION;
      this->verb_ = byte;
      return Event::EVENT_NONE;
    case TELNET_SB:
      this->state_ = State::STATE_SB;
      this->sub_len_ = 0;
      this->sub_overflow_ = false;
      return Event::EVENT_NONE;
    default:
      // NOP, GA and the other two-byte commands carry nothing for a serial port.
      this->state_ = State::STATE_DATA;
      return Event::EVENT_NONE;
  }
}

TelnetDecoder::Event TelnetDecoder::feed(uint8_t byte) {
  switch (this->state_) {
    case State::STATE_DATA:
      if (byte == TELNET_IAC) {
        this->state_ = State::STATE_IAC;
        return Event::EVENT_NONE;
      }
      this->data_ = byte;
      return Event::EVENT_DATA;
    case State::STATE_IAC:
      return this->after_iac_(byte);
    case State::STATE_OPTION:
      this->state_ = State::STATE_DATA;
      this->data_ = byte;
      return Event::EVENT_OPTION;
    case State::STATE_SB:
      if (byte == TELNET_IAC) {
        this->state_ = State::STATE_SB_IAC;
        return Event::EVENT_NONE;
      }
      break;
    case State::STATE_SB_IAC:
      if (byte == TELNET_SE) {
        this->state_ = State::STATE_DATA;
        return this->sub_overflow_ ? Event::EVENT_NONE : Event::EVENT_SUBNEGOTIATION;
      }
      if (byte != TELNET_IAC) {
        // [RFC 854] Only IAC SE ends a subnegotiation; drop it and take the byte as the command after IAC.
        return this->after_iac_(byte);
      }
      this->state_ = State::STATE_SB;
      break;
  }
  if (this->sub_len_ < SUB_SIZE) {
    this->sub_[this->sub_len_++] = byte;
  } else {
    this->sub_overflow_ = true;
  }
  return Event::EVENT_NONE;
}

size_t telnet_escape(const uint8_t *src, size_t len, uint8_t *dst, size_t room, size_t *used) {
  size_t in = 0;
  size_t out = 0;
  for (; in < len && out + (src[in] == TELNET_IAC ? 2 : 1) <= room; in++) {
    dst[out++] = src[in];
    if (src[in] == TELNET_IAC) {
      dst[out++] = TELNET_IAC;
    }
  }
  *used = in;
  return out;
}

size_t write_com_port(uint8_t *dst, uint8_t code, const uint8_t *value, size_t len) {
  dst[0] = TELNET_IAC;
  dst[1] = TELNET_SB;
  dst[2] = OPTION_COM_PORT;
  dst[3] = code;
  size_t used = 0;
  size_t n = 4 + telnet_escape(value, len, dst + 4, COM_PORT_COMMAND_MAX - 6, &used);
  dst[n++] = TELNET_IAC;
  dst[n++] = TELNET_SE;
  return n;
}

}  // namespace esphome::rfc2217_uart
