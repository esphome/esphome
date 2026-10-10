#include "panasonic_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote.panasonic";

static constexpr uint32_t HEADER_HIGH_US = 3502;
static constexpr uint32_t HEADER_LOW_US = 1750;
static constexpr uint32_t BIT_HIGH_US = 502;
static constexpr uint32_t BIT_ZERO_LOW_US = 400;
static constexpr uint32_t BIT_ONE_LOW_US = 1244;

void PanasonicProtocol::encode(RemoteTransmitData *dst, const PanasonicData &data) {
  dst->reserve(100);
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  dst->set_carrier_frequency(data.carrier_frequency);
  ESP_LOGD(TAG,
           "Encode Panasonic: address=%04" PRIX16 ", address_2=%02" PRIX8 ", command=%08" PRIX32
           ", nbits=%d, carrier_frequency=%ld",
           data.address, data.address_2, data.command, data.nbits, data.carrier_frequency);

  if (data.nbits != 48 && data.nbits != 56) {
    return;
  }

  uint32_t mask;
  for (mask = 1UL << 15; mask != 0; mask >>= 1) {
    if (data.address & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  if (data.nbits == 56) {
    for (mask = 1UL << 7; mask != 0; mask >>= 1) {
      if (data.address_2 & mask) {
        dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
      } else {
        dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
      }
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
  PanasonicData out{.nbits = 0, .address = 0, .address_2 = 0, .command = 0, .carrier_frequency = 35000};
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  out.nbits = src.size() / 2 - 2;

  if (out.nbits != 48 && out.nbits != 56) {
    return {};
  }

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

  if (out.nbits == 56) {
    for (mask = 1UL << 7; mask != 0; mask >>= 1) {
      if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
        out.address_2 |= mask;
      } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
        out.address_2 &= ~mask;
      } else {
        return {};
      }
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
  if (data.nbits == 48) {
    ESP_LOGI(TAG, "Received Panasonic: address=%04" PRIX16 ", command=%08" PRIX32, data.address, data.command);
  } else if (data.nbits == 56) {
    ESP_LOGI(TAG, "Received Panasonic: address=%04" PRIX16 ", address_2=%02" PRIX8 ", command=%08" PRIX32 ", nbits=%d ",
             data.address, data.address_2, data.command, data.nbits);
  }
}

}  // namespace esphome::remote_base
