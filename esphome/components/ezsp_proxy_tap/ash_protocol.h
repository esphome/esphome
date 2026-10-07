#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::ezsp_proxy_tap {

// ASH Protocol Constants
static constexpr uint8_t ASH_FLAG_BYTE = 0x7E;        // Frame delimiter
static constexpr uint8_t ASH_ESCAPE_BYTE = 0x7D;      // Escape/substitution byte
static constexpr uint8_t ASH_XOR_BYTE = 0x20;         // XOR mask for escaped bytes
static constexpr uint8_t ASH_SUBSTITUTE_BYTE = 0x18;  // Substitution for invalid bytes

static constexpr uint8_t ASH_XON_BYTE = 0x11;     // Resume transmission
static constexpr uint8_t ASH_XOFF_BYTE = 0x13;    // Pause transmission
static constexpr uint8_t ASH_CANCEL_BYTE = 0x1A;  // Discards the partial frame before it

// A reserved byte can never appear literally inside a frame; it is escaped as
// ESCAPE followed by the byte XOR 0x20. Rejecting frames that contain one is what
// eliminates most non-ASH traffic before its CRC is ever computed: real firmware
// images and Spinel payloads are dense in 0x11/0x13/0x18/0x1A.
inline bool ash_is_reserved(uint8_t byte) {
  return byte == ASH_FLAG_BYTE || byte == ASH_ESCAPE_BYTE || byte == ASH_XON_BYTE || byte == ASH_XOFF_BYTE ||
         byte == ASH_SUBSTITUTE_BYTE || byte == ASH_CANCEL_BYTE;
}

// ASH bounds a frame's Data Field at 128 bytes, so the largest body a scanner has to
// hold is that field plus the control byte and the two CRC bytes ahead of the closing
// delimiter. Byte stuffing happens on the wire only and is undone as bytes arrive, so it
// does not enlarge this.
static constexpr size_t ASH_MAX_DATA_FIELD_SIZE = 128;
static constexpr size_t MAX_ASH_FRAME_SIZE = 1 + ASH_MAX_DATA_FIELD_SIZE + 2;

// Protocol limits
static constexpr uint8_t ASH_MAX_SEQUENCE = 7;  // 3-bit sequence number (0-7)

// The frame CRC is CRC-CCITT: crc16be() with this initial value, transmitted big-endian.
// Note this is a different variant from the Kermit FCS that Spinel/HDLC-lite uses over
// the same 0x7E framing, so Spinel frames systematically fail this check.
static constexpr uint16_t ASH_CRC_INIT = 0xFFFF;

// An ACK is FLAG, control byte, two CRC bytes, FLAG. Every byte but the delimiters may
// need escaping, so the worst case is 2 + 3 * 2 = 8.
static constexpr size_t ASH_ACK_FRAME_MAX_SIZE = 8;

// Writes an ACK frame for `ack_num` into `output`, which must hold at least
// ASH_ACK_FRAME_MAX_SIZE bytes, and returns its length.
size_t ash_build_ack_frame(uint8_t *output, uint8_t ack_num);

}  // namespace esphome::ezsp_proxy_tap
