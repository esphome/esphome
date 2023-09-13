#include "panasonic_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.panasonic";

static const uint32_t HEADER_HIGH_US = 3502;
static const uint32_t HEADER_LOW_US = 1750;
static const uint32_t BIT_HIGH_US = 502;
static const uint32_t BIT_ZERO_LOW_US = 400;
static const uint32_t BIT_ONE_LOW_US = 1244;

void PanasonicProtocol::encode(RemoteTransmitData *dst, const PanasonicData &data) {
  dst->reserve(100);
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  dst->set_carrier_frequency(35000);

  uint32_t mask;
  for (mask = 1UL << 15; mask != 0; mask >>= 1) {
    if (data.address & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  for (mask = 1UL << 31; mask != 0; mask >>= 1) {
    if (data.command & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }
  dst->mark(BIT_HIGH_US);
}
optional<PanasonicData> PanasonicProtocol::decode(RemoteReceiveData src) {
  PanasonicData out{
      .address = 0,
      .command = 0,
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  uint32_t mask;
  for (mask = 1UL << 15; mask != 0; mask >>= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.address |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      out.address &= ~mask;
    } else {
      return {};
    }
  }

  for (mask = 1UL << 31; mask != 0; mask >>= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.command |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      out.command &= ~mask;
    } else {
      return {};
    }
  }

  return out;
}
void PanasonicProtocol::dump(const PanasonicData &data) {
  ESP_LOGI(TAG, "Received Panasonic: address=0x%04X, command=0x%08X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
