#include "samsung_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *TAG = "remote.samsung";

static const uint8_t NBITS = 32;
static const uint32_t HEADER_HIGH_US = 4500;
static const uint32_t HEADER_LOW_US = 4500;
static const uint32_t BIT_HIGH_US = 560;
static const uint32_t BIT_ONE_LOW_US = 1690;
static const uint32_t BIT_ZERO_LOW_US = 560;
static const uint32_t FOOTER_HIGH_US = 560;
static const uint32_t FOOTER_LOW_US = 560;

void SamsungProtocol::encode(RemoteTransmitData *dst, const SamsungData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(4 + NBITS * 2u);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  for (uint32_t mask = 1UL << (NBITS - 1); mask != 0; mask >>= 1) {
    if (data.data & mask)
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    else
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
  }

  dst->item(FOOTER_HIGH_US, FOOTER_LOW_US);
}
optional<SamsungData> SamsungProtocol::decode(RemoteReceiveData src) {
  SamsungData out{
      .data = 0,
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  for (uint8_t i = 0; i < NBITS; i++) {
    out.data <<= 1UL;
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.data |= 1UL;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      out.data |= 0UL;
    } else {
      return {};
    }
  }

  if (!src.expect_mark(FOOTER_HIGH_US))
    return {};
  return out;
}
void SamsungProtocol::dump(const SamsungData &data) { ESP_LOGD(TAG, "Received Samsung: data=0x%08X", data.data); }

}  // namespace remote_base
}  // namespace esphome
