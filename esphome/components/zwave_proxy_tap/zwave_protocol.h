#pragma once

#include <cstdint>

namespace esphome::zwave_proxy_tap {

// Z-Wave Serial API framing (INS12350). Unlike ASH, a data frame is length-prefixed
// rather than delimited:
//
//   SOF  LEN  TYPE  CMD  payload...  CHK
//
// LEN counts every byte after itself, the checksum included, so a frame occupies LEN + 2
// bytes on the wire. CHK is the XOR of LEN through the last payload byte, seeded with
// 0xFF. ACK, NAK and CAN stand alone as single bytes and carry no framing of their own.

static constexpr uint8_t ZWAVE_SOF_BYTE = 0x01;  // Start of a data frame
static constexpr uint8_t ZWAVE_ACK_BYTE = 0x06;  // The only byte this component ever sends

// TYPE field: which half of a request/response exchange the frame is
static constexpr uint8_t ZWAVE_FRAME_TYPE_REQUEST = 0x00;
static constexpr uint8_t ZWAVE_FRAME_TYPE_RESPONSE = 0x01;

// Smallest LEN the protocol can produce: TYPE, CMD and CHK, with no payload
static constexpr uint8_t ZWAVE_MIN_LENGTH = 3;

static constexpr uint8_t ZWAVE_CHECKSUM_INIT = 0xFF;

// The specification requires a receiver to abandon a data frame that has not completed
// this long after its SOF byte. Nothing in the framing marks where a frame ends, so
// without this a truncated frame would swallow the start of the next one.
static constexpr uint32_t ZWAVE_FRAME_TIMEOUT_MS = 1500;

}  // namespace esphome::zwave_proxy_tap
