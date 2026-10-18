#include "nec_protocol.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.nec");

static constexpr uint32_t HEADER_HIGH_US = 9000;
static constexpr uint32_t HEADER_LOW_US = 4500;
static constexpr uint32_t BIT_HIGH_US = 560;
static constexpr uint32_t BIT_ONE_LOW_US = 1690;
static constexpr uint32_t BIT_ZERO_LOW_US = 560;
static constexpr uint32_t REPEAT_HEADER_LOW_US = HEADER_LOW_US / 2;
// A held button repeats every 108 ms: the first gap depends on the frame's own length, later ones follow a repeat frame
static constexpr uint32_t REPEAT_PERIOD_US = 108000;
static constexpr uint32_t REPEAT_FRAME_US = HEADER_HIGH_US + REPEAT_HEADER_LOW_US + BIT_HIGH_US;
static constexpr uint32_t REPEAT_GAP_US = REPEAT_PERIOD_US - REPEAT_FRAME_US;
// Transmitters without an RMT peripheral hold an interrupt lock for the whole send, about 108 ms per repeat
static constexpr uint16_t MAX_COMMAND_REPEATS = 10;

void NECProtocol::encode(RemoteTransmitData *dst, const NECData &data) {
  ESP_LOGD(TAG, "Sending NEC: address=0x%04X, command=0x%04X command_repeats=%d", data.address, data.command,
           data.command_repeats);

  // The schema bounds literal values; a lambda can produce anything, so clamp here too
  const uint16_t repeats = clamp<uint16_t>(data.command_repeats, 1, MAX_COMMAND_REPEATS);
  // Frame: header pair, 32 bit pairs, stop mark. Each extra repeat: gap space, header pair, mark.
  dst->reserve(2 + 64 + 1 + (repeats - 1) * 4);
  dst->set_carrier_frequency(38000);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  uint32_t frame_us = HEADER_HIGH_US + HEADER_LOW_US + BIT_HIGH_US;

  for (uint16_t value : {data.address, data.command}) {
    for (uint16_t mask = 1; mask; mask <<= 1) {
      const uint32_t low_us = (value & mask) ? BIT_ONE_LOW_US : BIT_ZERO_LOW_US;
      dst->item(BIT_HIGH_US, low_us);
      frame_us += BIT_HIGH_US + low_us;
    }
  }

  dst->mark(BIT_HIGH_US);

  for (uint16_t repeat = 1; repeat < repeats; repeat++) {
    dst->space(repeat == 1 ? REPEAT_PERIOD_US - frame_us : REPEAT_GAP_US);
    dst->item(HEADER_HIGH_US, REPEAT_HEADER_LOW_US);
    dst->mark(BIT_HIGH_US);
  }
}
optional<NECData> NECProtocol::decode(RemoteReceiveData src) {
  NECData data{
      .address = 0,
      .command = 0,
      .command_repeats = 1,
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  for (uint16_t mask = 1; mask; mask <<= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      data.address |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      data.address &= ~mask;
    } else {
      return {};
    }
  }

  for (uint16_t mask = 1; mask; mask <<= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      data.command |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      data.command &= ~mask;
    } else {
      return {};
    }
  }

  // Repeat frames arrive as separate captures (receiver idle is far below the repeat gap) and are not decoded
  src.expect_mark(BIT_HIGH_US);
  return data;
}
void NECProtocol::dump(const NECData &data) {
  ESP_LOGI(TAG, "Received NEC: address=0x%04X, command=0x%04X command_repeats=%d", data.address, data.command,
           data.command_repeats);
}

}  // namespace esphome::remote_base
