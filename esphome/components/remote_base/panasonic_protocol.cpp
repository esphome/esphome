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
  dst->set_carrier_frequency(37000);

  uint16_t nbits = data.nbits;

  if (nbits != 48 && nbits != 56) {
    ESP_LOGI(TAG, "Transmit Panasonic: invalid number for nbits of %d. Only supported nbits are 48 or 56.", nbits);
    nbits = 48;
  }

  uint32_t mask;
  for (mask = 1UL << (nbits - 33); mask != 0; mask >>= 1) {
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
      .nbits = 0,
      .address = 0,
      .command = 0,
  };

  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  out.nbits = src.size() / 2 - 2;

  if (out.nbits != 48 && out.nbits != 56) {
    return {};
  }

  uint32_t mask;
  for (mask = 1UL << (out.nbits - 33); mask != 0; mask >>= 1) {
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
  if (data.nbits == 56) {
    ESP_LOGI(TAG, "Received Panasonic: address=%06" PRIX32 ", command=%" PRIX32 ", nbits=%d ", data.address,
             data.command, data.nbits);
  } else {
    ESP_LOGI(TAG, "Received Panasonic: address=%04" PRIX32 ", command=%" PRIX32 ", nbits=%d ", data.address,
             data.command, data.nbits);
  }
}

}  // namespace esphome::remote_base
