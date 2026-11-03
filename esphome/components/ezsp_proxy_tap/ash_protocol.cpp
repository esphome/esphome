#include "ash_protocol.h"

#include "esphome/core/helpers.h"

namespace esphome::ezsp_proxy_tap {

void ash_build_ack_frame(uint8_t *output, uint8_t ack_num) {
  // ACK control byte: 100nrPPP, where PPP is the next frame number expected
  const uint8_t control = 0x80 | (ack_num & ASH_MAX_SEQUENCE);

  output[0] = ASH_FLAG_BYTE;
  output[1] = control;
  put_be16(output + 2, crc16be(&control, 1, ASH_CRC_INIT));
  output[4] = ASH_FLAG_BYTE;
}

}  // namespace esphome::ezsp_proxy_tap
