#include "samsung_protocol.h"
#include "helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.samsung";

static const uint32_t HEADER_MARK_US = 4500;
static const uint32_t HEADER_SPACE_US = 4500;
static const uint32_t BIT_MARK_US = 560;
static const uint32_t BIT_ONE_SPACE_US = 1690;
static const uint32_t BIT_ZERO_SPACE_US = 560;
static const uint32_t FOOTER_MARK_US = 560;
static const uint32_t FOOTER_SPACE_US = 560;

DECLARE_SPACE_MSB_CODEC(codec)

void SamsungProtocol::encode(RemoteTransmitData *dst, const SamsungData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(4 + data.nbits * 2u);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  codec::encode(dst, data.data, data.nbits);
  dst->item(FOOTER_MARK_US, FOOTER_SPACE_US);
}

optional<SamsungData> SamsungProtocol::decode(RemoteReceiveData src) {
  SamsungData out{
      .data = 0,
      .nbits = 0,
  };

  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
    return {};

  out.nbits = codec::decode(src, out.data);

  if (out.nbits < 32 || !src.expect_mark(FOOTER_MARK_US))
    return {};

  return out;
}

void SamsungProtocol::dump(const SamsungData &data) {
  ESP_LOGD(TAG, "Received Samsung: data=0x%" PRIX64 ", nbits=%d", data.data, data.nbits);
}

}  // namespace remote_base
}  // namespace esphome
