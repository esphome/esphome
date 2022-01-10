#include "lg_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.lg";

static const uint32_t HEADER_HIGH_US = 8000;
static const uint32_t HEADER_LOW_US = 4000;
static const uint32_t BIT_MARK_US = 600;
static const uint32_t BIT_ONE_SPACE_US = 1600;
static const uint32_t BIT_ZERO_SPACE_US = 550;

void LGProtocol::encode(RemoteTransmitData *dst, const LGData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(2 + 2 * data.nbits + 2);
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  for (uint32_t mask = 1UL << (data.nbits - 1); mask != 0; mask >>= 1)
    dst->item(BIT_MARK_US, (data.data & mask) ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
  dst->mark(BIT_MARK_US);
}

optional<LGData> LGProtocol::decode(RemoteReceiveData src) {
  LGData out{
      .data = 0,
      .nbits = 0,
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  for (out.nbits = 0; out.nbits < 32; out.nbits++) {
    if (src.expect_item(BIT_MARK_US, BIT_ONE_SPACE_US)) {
      out.data = (out.data << 1) | 1;
    } else if (src.expect_item(BIT_MARK_US, BIT_ZERO_SPACE_US)) {
      out.data = (out.data << 1) | 0;
    } else if (out.nbits == 28) {
      return out;
    } else {
      return {};
    }
  }
  return out;
}

void LGProtocol::dump(const LGData &data) {
  ESP_LOGD(TAG, "Received LG: data=0x%08X, nbits=%d", data.data, data.nbits);
}

}  // namespace remote_base
}  // namespace esphome
