#include "sony_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.sony";

static const uint32_t HEADER_HIGH_US = 2400;
static const uint32_t HEADER_LOW_US = 600;
static const uint32_t BIT_ONE_HIGH_US = 1200;
static const uint32_t BIT_ZERO_HIGH_US = 600;
static const uint32_t BIT_LOW_US = 600;

void SonyProtocol::encode(RemoteTransmitData *dst, const SonyData &data) {
  dst->set_carrier_frequency(40000);
  dst->reserve(2 + data.nbits * 2u);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  for (uint32_t mask = 1UL << (data.nbits - 1); mask != 0; mask >>= 1) {
    if (data.data & mask) {
      dst->item(BIT_ONE_HIGH_US, BIT_LOW_US);
    } else {
      dst->item(BIT_ZERO_HIGH_US, BIT_LOW_US);
    }
  }
}
optional<SonyData> SonyProtocol::decode(RemoteReceiveData src) {
  SonyData out{
      .data = 0,
      .nbits = 0,
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  for (; out.nbits < 20; out.nbits++) {
    uint32_t bit;
    if (src.expect_mark(BIT_ONE_HIGH_US)) {
      bit = 1;
    } else if (src.expect_mark(BIT_ZERO_HIGH_US)) {
      bit = 0;
    } else if (out.nbits == 12 || out.nbits == 15) {
      return out;
    } else {
      return {};
    }

    out.data = (out.data << 1UL) | bit;
    if (src.expect_space(BIT_LOW_US)) {
      // nothing needs to be done
    } else if (src.peek_space_at_least(BIT_LOW_US)) {
      out.nbits += 1;
      if (out.nbits == 12 || out.nbits == 15 || out.nbits == 20)
        return out;
      return {};
    } else {
      return {};
    }
  }

  return out;
}
void SonyProtocol::dump(const SonyData &data) {
  ESP_LOGI(TAG, "Received Sony: data=0x%08X, nbits=%d", data.data, data.nbits);
}

}  // namespace remote_base
}  // namespace esphome
