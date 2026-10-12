#include "panasonic_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.panasonic");

static constexpr uint32_t HEADER_HIGH_US = 3502;
static constexpr uint32_t HEADER_LOW_US = 1750;
static constexpr uint32_t BIT_HIGH_US = 502;
static constexpr uint32_t BIT_ZERO_LOW_US = 400;
static constexpr uint32_t BIT_ONE_LOW_US = 1244;

void PanasonicProtocol::encode(RemoteTransmitData *dst, const PanasonicData &data) {
  // Header, one item per bit, trailing mark
  dst->reserve(3 + 2 * data.nbits);
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  dst->set_carrier_frequency(data.carrier_frequency);

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
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  const size_t nbits = src.size() / 2 - 2;
  if (nbits != 48 && nbits != 56) {
    return {};
  }
  PanasonicData out{};
  out.carrier_frequency = 35000;
  out.nbits = nbits;

  uint32_t mask;
  for (mask = 1UL << 15; mask != 0; mask >>= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.address |= mask;
    } else if (!src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      return {};
    }
  }

  if (out.nbits == 56) {
    for (mask = 1UL << 7; mask != 0; mask >>= 1) {
      if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
        out.address_2 |= mask;
      } else if (!src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
        return {};
      }
    }
  }

  for (mask = 1UL << 31; mask != 0; mask >>= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.command |= mask;
    } else if (!src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      return {};
    }
  }

  return out;
}
void PanasonicProtocol::dump(const PanasonicData &data) {
  ESP_LOGI(TAG, "Received Panasonic: nbits=%u, address=0x%04X, address_2=0x%02X, command=0x%08" PRIX32, data.nbits,
           data.address, data.address_2, data.command);
}

}  // namespace esphome::remote_base
