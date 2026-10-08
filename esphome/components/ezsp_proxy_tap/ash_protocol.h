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

// ASH bounds a frame's Data Field at 128 bytes, so the largest body a scanner has to
// accept is that field plus the control byte and the two CRC bytes ahead of the closing
// delimiter. Byte stuffing happens on the wire only and is undone as bytes arrive, so it
// does not enlarge this.
static constexpr size_t ASH_MAX_DATA_FIELD_SIZE = 128;
static constexpr size_t MAX_ASH_FRAME_SIZE = 1 + ASH_MAX_DATA_FIELD_SIZE + 2;

// The control byte, then the first Data Field byte (an RSTACK's version)
static constexpr size_t ASH_HEADER_SIZE = 2;

// Protocol limits
static constexpr uint8_t ASH_MAX_SEQUENCE = 7;  // 3-bit sequence number (0-7)

// The frame CRC is CRC-CCITT: crc16be() with this initial value, transmitted big-endian
static constexpr uint16_t ASH_CRC_INIT = 0xFFFF;

// FLAG, control byte, two CRC bytes, FLAG. None of the eight ACKs has a byte that needs
// escaping, so the size is fixed.
static constexpr size_t ASH_ACK_FRAME_SIZE = 5;

// Writes the ACK frame for `ack_num` into `output`
void ash_build_ack_frame(uint8_t *output, uint8_t ack_num);

}  // namespace esphome::ezsp_proxy_tap
