#include "ash_protocol.h"

#include "esphome/core/helpers.h"

namespace esphome::ezsp_proxy_tap {

// Appends a byte with ASH stuffing; the caller sized `output` for the worst case.
static void append_byte_stuffed(uint8_t *output, size_t &pos, uint8_t byte) {
  if (ash_is_reserved(byte)) {
    output[pos++] = ASH_ESCAPE_BYTE;
    output[pos++] = byte ^ ASH_XOR_BYTE;
  } else {
    output[pos++] = byte;
  }
}

size_t ash_build_ack_frame(uint8_t *output, uint8_t ack_num) {
  // ACK control byte: 100nrPPP, where PPP is the next frame number expected
  const uint8_t control = 0x80 | (ack_num & ASH_MAX_SEQUENCE);
  const uint16_t crc = crc16be(&control, 1, ASH_CRC_INIT);

  size_t pos = 0;
  output[pos++] = ASH_FLAG_BYTE;
  append_byte_stuffed(output, pos, control);
  append_byte_stuffed(output, pos, (crc >> 8) & 0xFF);
  append_byte_stuffed(output, pos, crc & 0xFF);
  output[pos++] = ASH_FLAG_BYTE;
  return pos;
}

}  // namespace esphome::ezsp_proxy_tap
