#include "toto_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.toto";

static const uint32_t PREAMBLE_HIGH_US = 6200;
static const uint32_t PREAMBLE_LOW_US = 2800;
static const uint32_t BIT_HIGH_US = 550;
static const uint32_t BIT_ONE_LOW_US = 1700;
static const uint32_t BIT_ZERO_LOW_US = 550;
static const uint32_t TOTO_HEADER = 0x2008;

void TotoProtocol::encode(RemoteTransmitData *dst, const TotoData &data) {
  uint32_t payload = 0;

  payload = data.rc_code_1 << 20;
  payload |= data.rc_code_2 << 16;
  payload |= data.command << 8;
  payload |= ((payload & 0xFF0000) >> 16) ^ ((payload & 0x00FF00) >> 8);

  dst->reserve(80);
  dst->set_carrier_frequency(38000);
  dst->item(PREAMBLE_HIGH_US, PREAMBLE_LOW_US);

  for (uint32_t mask = 1UL << 14; mask; mask >>= 1) {
    if (TOTO_HEADER & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  for (uint32_t mask = 1UL << 23; mask; mask >>= 1) {
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

  TotoData data{
      .rc_code_1 = 0,
      .rc_code_2 = 0,
      .command = 0,
  };

  if (!src.expect_item(PREAMBLE_HIGH_US, PREAMBLE_LOW_US)) {
    return {};
  }

  for (uint32_t mask = 1UL << 14; mask; mask >>= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      header |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      header &= ~mask;
    } else {
      return {};
    }
  }

  if (header != TOTO_HEADER) {
    return {};
  }

  for (uint32_t mask = 1UL << 23; mask; mask >>= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      payload |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      payload &= ~mask;
    } else {
      return {};
    }
  }

  if ((((payload & 0xFF0000) >> 16) ^ ((payload & 0x00FF00) >> 8)) != (payload & 0x0000FF)) {
    return {};
  }

  data.rc_code_1 = (payload & 0xF00000) >> 20;
  data.rc_code_2 = (payload & 0x0F0000) >> 16;
  data.command = (payload & 0x00FF00) >> 8;

  return data;
}
void TotoProtocol::dump(const TotoData &data) {
  ESP_LOGI(TAG, "Received Toto data: rc_code_1=0x%01X, rc_code_2=0x%01X, command=0x%02X", data.rc_code_1,
           data.rc_code_2, data.command);
}

}  // namespace remote_base
}  // namespace esphome
