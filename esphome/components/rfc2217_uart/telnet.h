#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::rfc2217_uart {

// [RFC 854]
static constexpr uint8_t TELNET_SE = 240;
static constexpr uint8_t TELNET_SB = 250;
static constexpr uint8_t TELNET_WILL = 251;
static constexpr uint8_t TELNET_WONT = 252;
static constexpr uint8_t TELNET_DO = 253;
static constexpr uint8_t TELNET_DONT = 254;
static constexpr uint8_t TELNET_IAC = 255;

// [RFC 856]
static constexpr uint8_t OPTION_BINARY = 0;
// [RFC 2217]
static constexpr uint8_t OPTION_COM_PORT = 44;

// [RFC 2217] COM-PORT commands as the client sends them; the server adds SERVER_OFFSET.
static constexpr uint8_t COM_SIGNATURE = 0;
static constexpr uint8_t COM_SET_BAUDRATE = 1;
static constexpr uint8_t COM_SET_DATASIZE = 2;
static constexpr uint8_t COM_SET_PARITY = 3;
static constexpr uint8_t COM_SET_STOPSIZE = 4;
static constexpr uint8_t COM_SET_CONTROL = 5;
static constexpr uint8_t COM_FLOWCONTROL_SUSPEND = 8;
static constexpr uint8_t COM_FLOWCONTROL_RESUME = 9;
static constexpr uint8_t COM_SET_LINESTATE_MASK = 10;
static constexpr uint8_t COM_SET_MODEMSTATE_MASK = 11;
static constexpr uint8_t COM_PURGE_DATA = 12;
static constexpr uint8_t SERVER_OFFSET = 100;

// [RFC 2217] SET-PARITY values.
static constexpr uint8_t PARITY_NONE = 1;
static constexpr uint8_t PARITY_ODD = 2;
static constexpr uint8_t PARITY_EVEN = 3;

// [RFC 2217] SET-CONTROL values.
static constexpr uint8_t CONTROL_NO_FLOW = 1;
static constexpr uint8_t CONTROL_BREAK_OFF = 6;
static constexpr uint8_t CONTROL_DTR_OFF = 9;
static constexpr uint8_t CONTROL_RTS_OFF = 12;
static constexpr uint8_t CONTROL_NO_FLOW_IN = 14;

/// Splits a Telnet stream into payload, option commands and subnegotiations.
/// Fed one byte at a time, so a sequence may end in a later read.
class TelnetDecoder {
 public:
  enum class Event : uint8_t {
    NONE,
    DATA,
    OPTION,
    SUBNEGOTIATION,
  };

  Event feed(uint8_t byte);
  void reset();
  /// No sequence is open: the next byte is payload unless it is IAC.
  bool idle() const { return this->state_ == State::DATA; }

  /// DATA: the payload byte.
  uint8_t data() const { return this->data_; }
  /// OPTION: WILL, WONT, DO or DONT, and the option.
  uint8_t verb() const { return this->verb_; }
  uint8_t option() const { return this->data_; }
  /// SUBNEGOTIATION: the bytes between IAC SB and IAC SE, IAC IAC undone.
  const uint8_t *sub() const { return this->sub_; }
  size_t sub_len() const { return this->sub_len_; }

  /// Longest subnegotiation kept; COM-PORT commands other than SIGNATURE need at most 6.
  static constexpr size_t SUB_SIZE = 16;

 protected:
  enum class State : uint8_t {
    DATA,
    IAC,
    OPTION,
    SB,
    SB_IAC,
  };

  Event after_iac_(uint8_t byte);

  State state_{State::DATA};
  uint8_t data_{0};
  uint8_t verb_{0};
  uint8_t sub_len_{0};
  // A subnegotiation longer than sub_ is skipped.
  bool sub_overflow_{false};
  uint8_t sub_[SUB_SIZE]{};
};

/// Copies payload into dst and doubles each IAC; never splits a pair. Returns the bytes written, *used the bytes taken.
size_t telnet_escape(const uint8_t *src, size_t len, uint8_t *dst, size_t room, size_t *used);

/// Longest COM-PORT command write_com_port() writes: a 4-byte value with every byte doubled.
static constexpr size_t COM_PORT_COMMAND_MAX = 14;

/// Writes IAC SB COM-PORT code value IAC SE into dst, which holds COM_PORT_COMMAND_MAX; returns its length.
size_t write_com_port(uint8_t *dst, uint8_t code, const uint8_t *value, size_t len);

}  // namespace esphome::rfc2217_uart
