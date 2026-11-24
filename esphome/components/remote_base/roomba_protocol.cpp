#include "roomba_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.roomba");

static constexpr uint8_t NBITS = 8;
static constexpr uint32_t BIT_ONE_HIGH_US = 3000;
static constexpr uint32_t BIT_ONE_LOW_US = 1000;
static constexpr uint32_t BIT_ZERO_HIGH_US = BIT_ONE_LOW_US;
static constexpr uint32_t BIT_ZERO_LOW_US = BIT_ONE_HIGH_US;
static constexpr BitTimings BIT_TIMINGS{.one_space = BIT_ONE_LOW_US,
                                        .zero_space = BIT_ZERO_LOW_US,
                                        .bit_order = BitOrder::BIT_ORDER_MSB_FIRST,
                                        .one_mark = BIT_ONE_HIGH_US,
                                        .zero_mark = BIT_ZERO_HIGH_US};

void RoombaProtocol::encode(RemoteTransmitData *dst, const RoombaData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(NBITS * 2u);
  dst->item_bytes<BIT_TIMINGS>(&data.data, 1);
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

}  // namespace esphome::remote_base
