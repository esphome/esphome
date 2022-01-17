#include "panasonic_protocol.h"
#include "helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.panasonic";

static const uint32_t HEADER_MARK_US = 3502;
static const uint32_t HEADER_SPACE_US = 1750;
static const uint32_t BIT_MARK_US = 502;
static const uint32_t BIT_ZERO_SPACE_US = 400;
static const uint32_t BIT_ONE_SPACE_US = 1244;
static const size_t REMOTE_DATA_SIZE = 2 + 2 * 16 + 2 * 32 + 2;

DECLARE_SPACE_MSB_CODEC(codec)

void PanasonicProtocol::encode(RemoteTransmitData *dst, const PanasonicData &data) {
  dst->set_carrier_frequency(35000);
  dst->reserve(REMOTE_DATA_SIZE);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  codec::encode(dst, data.address);
  codec::encode(dst, data.command);
  dst->mark(BIT_MARK_US);
}

static bool decode_data(RemoteReceiveData &src, PanasonicData &dst) {
  return codec::decode(src, dst.address) == 16 && codec::decode(src, dst.command) == 32;
}

optional<PanasonicData> PanasonicProtocol::decode(RemoteReceiveData src) {
  PanasonicData out{
      .address = 0,
      .command = 0,
  };
  if (src.has_size(REMOTE_DATA_SIZE) && src.expect_item(HEADER_MARK_US, HEADER_SPACE_US) && decode_data(src, out) &&
      src.expect_mark(BIT_MARK_US))
    return out;
  return {};
}

void PanasonicProtocol::dump(const PanasonicData &data) {
  ESP_LOGD(TAG, "Received Panasonic: address=0x%04X, command=0x%08X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
