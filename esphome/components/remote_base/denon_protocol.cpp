#include "denon_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.denon");

// Timings from Denon's published Pronto codes
static constexpr uint32_t CARRIER_FREQUENCY = 38000;
static constexpr uint32_t BIT_MARK_US = 263;
static constexpr uint32_t BIT_ZERO_SPACE_US = 789;
static constexpr uint32_t BIT_ONE_SPACE_US = 1841;
static constexpr uint32_t FRAME_GAP_US = 43572;

// Each frame is 15 bits sent least significant first: 5 address, 8 command, 2 trailing bits; then a stop mark
static constexpr uint8_t FRAME_BITS = 15;
static constexpr uint8_t ADDRESS_BITS = 5;
static constexpr uint16_t ADDRESS_MASK = (1U << ADDRESS_BITS) - 1;
static constexpr uint8_t TRAILER_SHIFT = 13;
// The second frame inverts the command and the trailing bits
static constexpr uint16_t SECOND_FRAME_MASK = 0x7FE0;

static void encode_frame(RemoteTransmitData *dst, uint16_t frame) {
  for (uint8_t i = 0; i < FRAME_BITS; i++) {
    dst->item(BIT_MARK_US, (frame >> i) & 1 ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
  }
  dst->mark(BIT_MARK_US);
}

void DenonProtocol::encode(RemoteTransmitData *dst, const DenonData &data) {
  dst->set_carrier_frequency(CARRIER_FREQUENCY);
  dst->reserve(2 * (2 * FRAME_BITS + 1) + 1);
  const uint16_t frame = (data.address & ADDRESS_MASK) | (data.command << ADDRESS_BITS);
  encode_frame(dst, frame);
  dst->space(FRAME_GAP_US);
  encode_frame(dst, frame ^ SECOND_FRAME_MASK);
}

optional<DenonData> DenonProtocol::decode(RemoteReceiveData src) {
  uint16_t frame = 0;
  for (uint8_t i = 0; i < FRAME_BITS; i++) {
    if (src.expect_item(BIT_MARK_US, BIT_ONE_SPACE_US)) {
      frame |= 1U << i;
    } else if (!src.expect_item(BIT_MARK_US, BIT_ZERO_SPACE_US)) {
      return {};
    }
  }
  // Only the first frame of a pair ends in 00; the inverted second frame ends in 11
  if (!src.expect_mark(BIT_MARK_US) || (frame >> TRAILER_SHIFT) != 0)
    return {};
  return DenonData{
      .address = static_cast<uint8_t>(frame & ADDRESS_MASK),
      .command = static_cast<uint8_t>(frame >> ADDRESS_BITS),
  };
}

void DenonProtocol::dump(const DenonData &data) {
  ESP_LOGI(TAG, "Received Denon: address=%u, command=%u", data.address, data.command);
}

}  // namespace esphome::remote_base
