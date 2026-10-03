#include "nec_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote.nec";

static constexpr uint32_t HEADER_HIGH_US = 9000;
static constexpr uint32_t HEADER_LOW_US = 4500;
static constexpr uint32_t BIT_HIGH_US = 560;
static constexpr uint32_t BIT_ONE_LOW_US = 1690;
static constexpr uint32_t BIT_ZERO_LOW_US = 560;

void NECProtocol::encode(RemoteTransmitData *dst, const NECData &data) {
  ESP_LOGD(TAG, "Sending NEC: address=0x%04X, command=0x%04X command_repeats=%d", data.address, data.command,
           data.command_repeats);

  dst->reserve(2 + 32 + 32 * data.command_repeats + 2);
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
    dst->space(40500);
    for (uint16_t repeats = 1; repeats < data.command_repeats; repeats++) {
      dst->item(HEADER_HIGH_US, HEADER_LOW_US / 2);
      dst->mark(BIT_HIGH_US);
      if (repeats + 1 < data.command_repeats) {
        dst->space(96187);
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

  if (!src.expect_mark(BIT_HIGH_US)) {
    return {};
  }

  while (src.expect_space(40500) || src.expect_space(96187)) {
    if (src.expect_item(HEADER_HIGH_US, HEADER_LOW_US / 2) && src.expect_mark(BIT_HIGH_US)) {
      data.command_repeats += 1;
    } else {
      break;
    }
  }

  return data;
}
void NECProtocol::dump(const NECData &data) {
  ESP_LOGI(TAG, "Received NEC: address=0x%04X, command=0x%04X command_repeats=%d", data.address, data.command,
           data.command_repeats);
}

}  // namespace esphome::remote_base
