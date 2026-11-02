#include "toto_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.toto");

static constexpr uint32_t PREAMBLE_HIGH_US = 6000;
static constexpr uint32_t PREAMBLE_LOW_US = 3000;
static constexpr uint32_t BIT_HIGH_US = 600;
static constexpr uint32_t BIT_ONE_LOW_US = 1667;
static constexpr uint32_t BIT_ZERO_LOW_US = 520;
static constexpr uint16_t TOTO_HEADER = 0x0802;

void TotoProtocol::encode(RemoteTransmitData *dst, const TotoData &code) {
  uint8_t checksum = ((code & 0xFF00) >> 8) ^ (code & 0x00FF);
  uint32_t payload = code | (checksum << 16);
  dst->reserve(80);
  dst->set_carrier_frequency(38000);
  dst->item(PREAMBLE_HIGH_US, PREAMBLE_LOW_US);

  for (uint16_t mask = 1UL; mask <= (1UL << 14); mask <<= 1) {
    if (TOTO_HEADER & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  for (uint32_t mask = 1UL; mask <= (1UL << 23); mask <<= 1) {
    if (payload & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  dst->mark(BIT_HIGH_US);
}
optional<TotoData> TotoProtocol::decode(RemoteReceiveData src) {
  uint16_t header = 0;
  uint32_t payload = 0;

  if (!src.expect_item(PREAMBLE_HIGH_US, PREAMBLE_LOW_US)) {
    return {};
  }

  for (uint32_t mask = 1UL; mask <= (1UL << 14); mask <<= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      header |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
    } else {
      return {};
    }
  }

  if (header != TOTO_HEADER) {
    return {};
  }

  for (uint32_t mask = 1UL; mask <= (1UL << 23); mask <<= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      payload |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
    } else {
      return {};
    }
  }

  if (((payload & 0xFF0000) >> 16) != (((payload & 0x00FF00) >> 8) ^ (payload & 0x0000FF))) {
    return {};
  }

  return payload & 0xffff;
}
void TotoProtocol::dump(const TotoData &code) { ESP_LOGI(TAG, "Received Toto code 0x%04X", code); }

}  // namespace esphome::remote_base
