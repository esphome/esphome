#include "sony_protocol.h"
#include "helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.sony";

static const uint32_t HEADER_MARK_US = 2400;
static const uint32_t HEADER_SPACE_US = 600;
static const uint32_t BIT_ONE_MARK_US = 1200;
static const uint32_t BIT_ZERO_MARK_US = 600;
static const uint32_t BIT_SPACE_US = 600;

DECLARE_MARK_MSB_CODEC(codec)

void SonyProtocol::encode(RemoteTransmitData *dst, const SonyData &data) {
  dst->set_carrier_frequency(40000);
  dst->reserve(2 + data.nbits * 2u);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  codec::encode(dst, data.data, data.nbits);
}

optional<SonyData> SonyProtocol::decode(RemoteReceiveData src) {
  SonyData out{
      .data = 0,
      .nbits = 0,
  };
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
    return {};
  out.nbits = codec::decode(src, out.data, 20);
  if (out.nbits == 12 || out.nbits == 15 || out.nbits == 20)
    return out;
  return {};
}

void SonyProtocol::dump(const SonyData &data) {
  ESP_LOGD(TAG, "Received Sony: data=0x%08X, nbits=%d", data.data, data.nbits);
}

}  // namespace remote_base
}  // namespace esphome
