#include "nec_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.nec";

static const uint32_t HEADER_HIGH_US = 9000;
static const uint32_t HEADER_LOW_US = 4500;
static const uint32_t BIT_HIGH_US = 560;
static const uint32_t BIT_ONE_LOW_US = 1690;
static const uint32_t BIT_ZERO_LOW_US = 560;

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

  for (uint16_t repeats = 0; repeats < data.command_repeats; repeats++) {
    for (uint16_t mask = 1; mask; mask <<= 1) {
      if (data.command & mask) {
        dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
      } else {
        dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
      }
    }
  }

  dst->mark(BIT_HIGH_US);
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

  while (src.peek_item(BIT_HIGH_US, BIT_ONE_LOW_US) || src.peek_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
    uint16_t command = 0;
    for (uint16_t mask = 1; mask; mask <<= 1) {
      if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
        command |= mask;
      } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
        command &= ~mask;
      } else {
        return {};
      }
    }

    // Make sure the extra/repeated data matches original command
    if (command != data.command) {
      return {};
    }

    data.command_repeats += 1;
  }

  src.expect_mark(BIT_HIGH_US);
  return data;
}
void NECProtocol::dump(const NECData &data) {
  ESP_LOGI(TAG, "Received NEC: address=0x%04X, command=0x%04X command_repeats=%d", data.address, data.command,
           data.command_repeats);
}

}  // namespace remote_base
}  // namespace esphome
