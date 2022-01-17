#include "lg_protocol.h"
#include "helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.lg";

static const uint32_t HEADER_MARK_US = 8000;
static const uint32_t HEADER_SPACE_US = 4000;
static const uint32_t BIT_MARK_US = 600;
static const uint32_t BIT_ONE_SPACE_US = 1600;
static const uint32_t BIT_ZERO_SPACE_US = 550;

USE_SPACE_MSB_CODEC(codec)

void LGProtocol::encode(RemoteTransmitData *dst, const LGData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(2 + 2 * data.nbits + 2);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  codec::encode(dst, data.data, data.nbits);
  for (uint32_t mask = 1UL << (data.nbits - 1); mask != 0; mask >>= 1)
    dst->item(BIT_MARK_US, (data.data & mask) ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
  dst->mark(BIT_MARK_US);
}

optional<LGData> LGProtocol::decode(RemoteReceiveData src) {
  LGData out{
      .data = 0,
      .nbits = 0,
  };
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
    return {};
  out.nbits = codec::decode(src, out.data);
  if (out.nbits == 28 || out.nbits == 32)
    return out;
  return {};
}

void LGProtocol::dump(const LGData &data) {
  ESP_LOGD(TAG, "Received LG: data=0x%08X, nbits=%d", data.data, data.nbits);
}

}  // namespace remote_base
}  // namespace esphome
