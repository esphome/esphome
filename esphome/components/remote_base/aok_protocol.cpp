#include "aok_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.aok");

// A frame is a long start mark, then 64 bits, then a long gap. A one bit is a long mark and a short space,
// a zero bit is a short mark and a long space. The bits are: header (8), remote id (24), channels (16),
// command (8) and checksum (8), most significant bit first.
static constexpr uint8_t NBITS = 64;
static constexpr uint32_t START_HIGH_US = 5000;
static constexpr uint32_t LONG_US = 600;
static constexpr uint32_t SHORT_US = 285;
static constexpr uint8_t HEADER = 0xA3;
static constexpr uint32_t REMOTE_ID_MASK = 0xFFFFFF;

// The sum of the six bytes between the header and the checksum
static uint8_t checksum(uint64_t frame) {
  uint8_t sum = 0;
  for (uint8_t shift = 8; shift < 56; shift += 8) {
    sum += frame >> shift;
  }
  return sum;
}

void AOKProtocol::encode(RemoteTransmitData *dst, const AOKData &data) {
  uint64_t frame = (uint64_t(HEADER) << 56) | (uint64_t(data.remote_id & REMOTE_ID_MASK) << 32) |
                   (uint64_t(data.channels) << 16) | (uint64_t(data.command) << 8);
  frame |= checksum(frame);

  dst->reserve((NBITS + 2) * 2);
  dst->item(START_HIGH_US, LONG_US);
  for (uint64_t mask = 1ULL << (NBITS - 1); mask; mask >>= 1) {
    if (frame & mask) {
      dst->item(LONG_US, SHORT_US);
    } else {
      dst->item(SHORT_US, LONG_US);
    }
  }
  dst->item(LONG_US, START_HIGH_US);
}

optional<AOKData> AOKProtocol::decode(RemoteReceiveData src) {
  // A buffer can hold noise and several repeats of a frame, so look for the first good one. After any
  // failure the loop restarts at the next timing, or where the failed frame stopped.
  while (src.is_valid()) {
    if (!src.expect_pulse_with_gap(START_HIGH_US, LONG_US)) {
      src.advance();
      continue;
    }

    uint64_t frame = 0;
    uint8_t bits = 0;
    for (; bits < NBITS; bits++) {
      frame <<= 1;
      if (src.expect_item(LONG_US, SHORT_US)) {
        frame |= 1;
      } else if (!src.expect_item(SHORT_US, LONG_US)) {
        break;
      }
    }

    if (bits == NBITS && uint8_t(frame >> 56) == HEADER && uint8_t(frame) == checksum(frame)) {
      return AOKData{
          .remote_id = uint32_t(frame >> 32) & REMOTE_ID_MASK,
          .channels = uint16_t(frame >> 16),
          .command = uint8_t(frame >> 8),
      };
    }
  }
  return {};
}

void AOKProtocol::dump(const AOKData &data) {
  ESP_LOGI(TAG, "Received A-OK: remote_id=0x%06" PRIX32 ", channels=0x%04X, command=0x%02X", data.remote_id,
           data.channels, data.command);
}

}  // namespace esphome::remote_base
