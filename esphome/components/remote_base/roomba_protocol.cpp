#include "roomba_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.roomba";

static const uint8_t NBITS = 8;
static const uint32_t BIT_ONE_HIGH_US = 3000;
static const uint32_t BIT_ONE_LOW_US = 1000;
static const uint32_t BIT_ZERO_HIGH_US = BIT_ONE_LOW_US;
static const uint32_t BIT_ZERO_LOW_US = BIT_ONE_HIGH_US;

void RoombaProtocol::encode(RemoteTransmitData *dst, const RoombaData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(NBITS * 2u);

  for (uint32_t mask = 1UL << (NBITS - 1); mask != 0; mask >>= 1) {
    if (data.data & mask) {
      dst->item(BIT_ONE_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_ZERO_HIGH_US, BIT_ZERO_LOW_US);
    }
  }
}
optional<RoombaData> RoombaProtocol::decode(RemoteReceiveData src) {
  RoombaData out{.data = 0};

  for (uint8_t i = 0; i < (NBITS - 1); i++) {
    out.data <<= 1UL;
    if (src.expect_item(BIT_ONE_HIGH_US, BIT_ONE_LOW_US)) {
      out.data |= 1UL;
    } else if (src.expect_item(BIT_ZERO_HIGH_US, BIT_ZERO_LOW_US)) {
      out.data |= 0UL;
    } else {
      return {};
    }
  }

  // not possible to measure space on last bit, check only mark
  out.data <<= 1UL;
  if (src.expect_mark(BIT_ONE_HIGH_US)) {
    out.data |= 1UL;
  } else if (src.expect_mark(BIT_ZERO_HIGH_US)) {
    out.data |= 0UL;
  } else {
    return {};
  }

  return out;
}
void RoombaProtocol::dump(const RoombaData &data) { ESP_LOGD(TAG, "Received Roomba: data=0x%02X", data.data); }

}  // namespace remote_base
}  // namespace esphome
