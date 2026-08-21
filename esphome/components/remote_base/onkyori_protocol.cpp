#include "onkyori_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote.onkyori";

static constexpr uint8_t NBITS = 12;
static constexpr uint32_t HEADER_HIGH_US = 3000;
static constexpr uint32_t HEADER_LOW_US = 1000;
static constexpr uint32_t BIT_HIGH_US = 1000;
static constexpr uint32_t BIT_ONE_LOW_US = 2000;
static constexpr uint32_t BIT_ZERO_LOW_US = 1000;
static constexpr uint32_t TRAILER_HIGH_US = 1000;

void OnkyoRIProtocol::encode(RemoteTransmitData *dst, const OnkyoRIData &data) {
  // RI is a baseband signal on a wire, so it has no carrier to modulate.
  dst->set_carrier_frequency(0);
  dst->reserve(2 + NBITS * 2u + 1);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  for (uint32_t mask = 1UL << (NBITS - 1); mask != 0; mask >>= 1) {
    dst->item(BIT_HIGH_US, (data.data & mask) != 0 ? BIT_ONE_LOW_US : BIT_ZERO_LOW_US);
  }

  dst->mark(TRAILER_HIGH_US);
}

optional<OnkyoRIData> OnkyoRIProtocol::decode(RemoteReceiveData src) {
  OnkyoRIData out{.data = 0};

  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  for (uint8_t i = 0; i < NBITS; i++) {
    out.data <<= 1;
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.data |= 1;
    } else if (!src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      return {};
    }
  }

  if (!src.expect_mark(TRAILER_HIGH_US))
    return {};

  return out;
}

void OnkyoRIProtocol::dump(const OnkyoRIData &data) { ESP_LOGI(TAG, "Received OnkyoRI: data=0x%03X", data.data); }

}  // namespace esphome::remote_base
