#include "samsung_protocol.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.samsung";

static const uint32_t HEADER_HIGH_US = 4500;
static const uint32_t HEADER_LOW_US = 4500;
static const uint32_t BIT_HIGH_US = 560;
static const uint32_t BIT_ONE_LOW_US = 1690;
static const uint32_t BIT_ZERO_LOW_US = 560;
static const uint32_t FOOTER_HIGH_US = 560;
static const uint32_t FOOTER_LOW_US = 560;

void SamsungProtocol::encode(RemoteTransmitData *dst, const SamsungData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(4 + data.nbits * 2u);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  for (uint8_t bit = data.nbits; bit > 0; bit--) {
    if ((data.data >> (bit - 1)) & 1) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  dst->item(FOOTER_HIGH_US, FOOTER_LOW_US);
}
optional<SamsungData> SamsungProtocol::decode(RemoteReceiveData src) {
  SamsungData out{
      .data = 0,
      .nbits = 0,
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  for (out.nbits = 0; out.nbits < 64; out.nbits++) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.data = (out.data << 1) | 1;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      out.data = (out.data << 1) | 0;
    } else if (out.nbits >= 31) {
      if (!src.expect_mark(FOOTER_HIGH_US))
        return {};
      return out;
    } else {
      return {};
    }
  }

  if (!src.expect_mark(FOOTER_HIGH_US))
    return {};
  return out;
}
void SamsungProtocol::dump(const SamsungData &data) {
  ESP_LOGD(TAG, "Received Samsung: data=0x%" PRIX64 ", nbits=%d", data.data, data.nbits);
}

}  // namespace remote_base
}  // namespace esphome
