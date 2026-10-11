#include "nec_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.nec");

static constexpr uint32_t HEADER_HIGH_US = 9000;
static constexpr uint32_t HEADER_LOW_US = 4500;
static constexpr uint32_t BIT_HIGH_US = 560;
static constexpr uint32_t BIT_ONE_LOW_US = 1690;
static constexpr uint32_t BIT_ZERO_LOW_US = 560;
static constexpr uint32_t REPEAT_HEADER_LOW_US = HEADER_LOW_US / 2;
static constexpr uint32_t FIRST_REPEAT_GAP_US = 40500;
static constexpr uint32_t REPEAT_GAP_US = 96187;

void NECProtocol::encode(RemoteTransmitData *dst, const NECData &data) {
  ESP_LOGD(TAG, "Sending NEC: address=0x%04X, command=0x%04X command_repeats=%d", data.address, data.command,
           data.command_repeats);

  // Frame: header pair, 32 bit pairs, stop mark. Each extra repeat: gap space, header pair, mark.
  const uint32_t extra_repeats = data.command_repeats > 1 ? data.command_repeats - 1 : 0;
  dst->reserve(2 + 64 + 1 + extra_repeats * 4);
  dst->set_carrier_frequency(38000);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  for (uint16_t mask = 1; mask; mask <<= 1) {
    if (data.address & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  for (uint16_t mask = 1; mask; mask <<= 1) {
    if (data.command & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  dst->mark(BIT_HIGH_US);

  if (data.command_repeats > 1) {
    dst->space(FIRST_REPEAT_GAP_US);
    for (uint16_t repeats = 1; repeats < data.command_repeats; repeats++) {
      dst->item(HEADER_HIGH_US, REPEAT_HEADER_LOW_US);
      dst->mark(BIT_HIGH_US);
      if (repeats + 1 < data.command_repeats) {
        dst->space(REPEAT_GAP_US);
      }
    }
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
