#include "ash_protocol.h"

namespace esphome::zigbee_proxy {

uint16_t ash_crc16(const uint8_t *data, size_t length, uint16_t init) {
  uint16_t crc = init;
  for (size_t i = 0; i < length; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

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
  const uint16_t crc = ash_crc16(&control, 1);

  size_t pos = 0;
  output[pos++] = ASH_FLAG_BYTE;
  append_byte_stuffed(output, pos, control);
  append_byte_stuffed(output, pos, (crc >> 8) & 0xFF);
  append_byte_stuffed(output, pos, crc & 0xFF);
  output[pos++] = ASH_FLAG_BYTE;
  return pos;
}

}  // namespace esphome::zigbee_proxy
