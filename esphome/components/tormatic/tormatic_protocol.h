#pragma once

#include "esphome/components/cover/cover.h"

/**
 * This file implements the UART protocol spoken over the on-board Micro-USB
 * (Type B) connector of Tormatic and Novoferm gates manufactured as of 2016.
 * All communication is initiated by the component. The unit doesn't send data
 * without being asked first.
 *
 * There are two main message types: status requests and commands.
 *
 * Querying the gate's status:
 *
 * | sequence  |        length       |    type   |       payload       |
 * | 0xF3 0xCB | 0x00 0x00 0x00 0x06 | 0x01 0x04 | 0x00 0x0A 0x00 0x01 |
 * | 0xF3 0xCB | 0x00 0x00 0x00 0x05 | 0x01 0x04 | 0x02 0x03 0x00      |
 *
 * This request asks for the gate status (0x0A); the only other value observed
 * in the request was 0x0B, but replies were always zero. Presumably this
 * queries another sensor on the unit like a safety breaker, but this is not
 * relevant for an esphome cover component.
 *
 * The second byte of the reply is set to 0x03 when the gate is in fully open
 * position. Other valid values for the second byte are: (0x0) Paused, (0x1)
 * Closed, (0x2) Ventilating, (0x3) Opened, (0x4) Opening, (0x5) Closing. The
 * meaning of the other bytes is currently unknown and ignored by the component.
 *
 * Controlling the gate:
 *
 * | sequence  |        length       |    type   |       payload       |
 * | 0x40 0xFF | 0x00 0x00 0x00 0x06 | 0x01 0x06 | 0x00 0x0A 0x00 0x03 |
 * | 0x40 0xFF | 0x00 0x00 0x00 0x06 | 0x01 0x06 | 0x00 0x0A 0x00 0x03 |
 *
 * The unit acks any commands by echoing back the message in full. However,
 * this does _not_ mean the gate has started closing. The component only
 * considers status replies as authoritative and simply fires off commands,
 * ignoring the echoed messages.
 *
 * The payload structure is as follows: [0x00, 0x0A] (gate), followed by
 * one of the states normally carried in status replies: (0x0) Pause, (0x1)
 * Close, (0x2) Ventilate (open ~20%), (0x3) Open/high-torque reverse. The
 * protocol implementation in this file simply reuses the GateStatus enum
 * for this purpose.
 */

namespace esphome {
namespace tormatic {

using namespace esphome::cover;

// MessageType is the type of message that follows the MessageHeader.
enum MessageType : uint16_t {
  STATUS = 0x0104,
  COMMAND = 0x0106,
};

inline const char *message_type_to_str(MessageType t) {
  switch (t) {
    case STATUS:
      return "Status";
    case COMMAND:
      return "Command";
    default:
      return "Unknown";
  }
}

// MessageHeader appears at the start of every message, both requests and replies.
struct MessageHeader {
  uint16_t seq;
  uint32_t len;
  MessageType type;

  MessageHeader() = default;
  MessageHeader(MessageType type, uint16_t seq, uint32_t payload_size) {
    this->type = type;
    this->seq = seq;
    // len includes the length of the type field. It was
    // included in MessageHeader to avoid having to parse
    // it as part of the payload.
    this->len = payload_size + sizeof(this->type);
  }

  std::string print() {
    return str_sprintf("MessageHeader: seq %d, len %d, type %s", this->seq, this->len, message_type_to_str(this->type));
  }

  void byteswap() {
    this->len = convert_big_endian(this->len);
    this->seq = convert_big_endian(this->seq);
    this->type = convert_big_endian(this->type);
  }

  // payload_size returns the amount of payload bytes to be read from the uart
  // buffer after reading the header.
  uint32_t payload_size() { return this->len - sizeof(this->type); }
} __attribute__((packed));

// StatusType denotes which 'page' of information needs to be retrieved.
// On my Novoferm 423, only the GATE status type returns values, Unknown
// only contains zeroes.
enum StatusType : uint16_t {
  GATE = 0x0A,
  UNKNOWN = 0x0B,
};

// GateStatus defines the current state of the gate, received in a StatusReply
// and sent in a Command.
enum GateStatus : uint8_t {
  PAUSED,
  CLOSED,
  VENTILATING,
  OPENED,
  OPENING,
  CLOSING,
};

inline CoverOperation gate_status_to_cover_operation(GateStatus s) {
  switch (s) {
    case OPENING:
      return COVER_OPERATION_OPENING;
    case CLOSING:
      return COVER_OPERATION_CLOSING;
    case OPENED:
    case CLOSED:
    case PAUSED:
    case VENTILATING:
      return COVER_OPERATION_IDLE;
  }
  return COVER_OPERATION_IDLE;
}

inline const char *gate_status_to_str(GateStatus s) {
  switch (s) {
    case PAUSED:
      return "Paused";
    case CLOSED:
      return "Closed";
    case VENTILATING:
      return "Ventilating";
    case OPENED:
      return "Opened";
    case OPENING:
      return "Opening";
    case CLOSING:
      return "Closing";
    default:
      return "Unknown";
  }
}

// A StatusRequest is sent to request the gate's current status.
struct StatusRequest {
  StatusType type;
  uint16_t trailer = 0x1;

  StatusRequest() = default;
  StatusRequest(StatusType type) { this->type = type; }

  void byteswap() {
    this->type = convert_big_endian(this->type);
    this->trailer = convert_big_endian(this->trailer);
  }
} __attribute__((packed));

// StatusReply is received from the unit in response to a StatusRequest.
struct StatusReply {
  uint8_t ack = 0x2;
  GateStatus state;
  uint8_t trailer = 0x0;

  std::string print() { return str_sprintf("StatusReply: state %s", gate_status_to_str(this->state)); }

  void byteswap(){};
} __attribute__((packed));

// Serialize the given object to a new byte vector.
// Invokes the object's byteswap() method.
template<typename T> std::vector<uint8_t> serialize(T obj) {
  obj.byteswap();

  std::vector<uint8_t> out(sizeof(T));
  memcpy(out.data(), &obj, sizeof(T));

  return out;
}

// Command tells the gate to start or stop moving.
// It is echoed back by the unit on success.
struct CommandRequestReply {
  // The part of the unit to control. For now only the gate is supported.
  StatusType type = GATE;
  uint8_t pad = 0x0;
  // The desired state:
  // PAUSED = stop
  // VENTILATING = move to ~20% open
  // CLOSED = close
  // OPENED = open/high-torque reverse when closing
  GateStatus state;

  CommandRequestReply() = default;
  CommandRequestReply(GateStatus state) { this->state = state; }

  std::string print() { return str_sprintf("CommandRequestReply: state %s", gate_status_to_str(this->state)); }

  void byteswap() { this->type = convert_big_endian(this->type); }
} __attribute__((packed));

}  // namespace tormatic
}  // namespace esphome
