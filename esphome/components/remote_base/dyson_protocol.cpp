#include "dyson_protocol.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.dyson";

// pulsewidth [µs]
constexpr uint32_t PW_MARK_US = 780;
constexpr uint32_t PW_SHORT_US = 720;
constexpr uint32_t PW_LONG_US = 1500;
constexpr uint32_t PW_START_US = 2280;

// MSB of 15 bit dyson code
constexpr uint16_t MSB_DYSON = (1 << 14);

// required symbols in transmit buffer = (start_symbol + 15 data_symbols)
constexpr uint32_t N_SYMBOLS_REQ = 2u * (1 + 15);

void DysonProtocol::encode(RemoteTransmitData *dst, const DysonData &data) {
  uint32_t raw_code = (data.code << 2) + (data.index & 3);
  dst->set_carrier_frequency(36000);
  dst->reserve(N_SYMBOLS_REQ + 1);
  dst->item(PW_START_US, PW_SHORT_US);
  for (uint16_t mask = MSB_DYSON; mask != 0; mask >>= 1) {
    if (mask == (mask & raw_code)) {
      dst->item(PW_MARK_US, PW_LONG_US);
    } else {
      dst->item(PW_MARK_US, PW_SHORT_US);
    }
  }
  dst->mark(PW_MARK_US);  // final carrier pulse
}

optional<DysonData> DysonProtocol::decode(RemoteReceiveData src) {
  uint32_t n_received = static_cast<uint32_t>(src.size());
  uint16_t raw_code = 0;
  DysonData data{
      .code = 0,
      .index = 0,
  };
  if (n_received < N_SYMBOLS_REQ)
    return {};  // invalid frame length
  if (!src.expect_item(PW_START_US, PW_SHORT_US))
    return {};  // start not found
  for (uint16_t mask = MSB_DYSON; mask != 0; mask >>= 1) {
    if (src.expect_item(PW_MARK_US, PW_SHORT_US)) {
      raw_code &= ~mask;  // zero detected
    } else if (src.expect_item(PW_MARK_US, PW_LONG_US)) {
      raw_code |= mask;  // one detected
    } else {
      return {};  // invalid data item
    }
  }
  data.code = raw_code >> 2;          // extract button code
  data.index = raw_code & 3;          // extract rolling index
  if (src.expect_mark(PW_MARK_US)) {  // check total length
    return data;
  }
  return {};  // frame not complete
}

void DysonProtocol::dump(const DysonData &data) {
  ESP_LOGI(TAG, "Dyson: code=0x%x rolling index=%d", data.code, data.index);
}

}  // namespace remote_base
}  // namespace esphome
