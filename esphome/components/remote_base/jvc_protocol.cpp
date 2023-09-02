#include "jvc_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.jvc";

static const uint8_t NBITS = 16;
static const uint32_t HEADER_HIGH_US = 8400;
static const uint32_t HEADER_LOW_US = 4200;
static const uint32_t BIT_ONE_LOW_US = 1725;
static const uint32_t BIT_ZERO_LOW_US = 525;
static const uint32_t BIT_HIGH_US = 525;

void JVCProtocol::encode(RemoteTransmitData *dst, const JVCData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(2 + NBITS * 2u);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  for (uint32_t mask = 1UL << (NBITS - 1); mask != 0; mask >>= 1) {
    if (data.data & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  dst->mark(BIT_HIGH_US);
}
optional<JVCData> JVCProtocol::decode(RemoteReceiveData src) {
  JVCData out{.data = 0};
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
  return out;
}
void JVCProtocol::dump(const JVCData &data) { ESP_LOGI(TAG, "Received JVC: data=0x%04X", data.data); }

}  // namespace remote_base
}  // namespace esphome
