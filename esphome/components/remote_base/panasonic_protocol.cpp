#include "panasonic_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.panasonic";

static const uint32_t HEADER_HIGH_US = 3502;
static const uint32_t HEADER_LOW_US = 1750;
static const uint32_t BIT_MARK_US = 502;
static const uint32_t BIT_ZERO_SPACE_US = 400;
static const uint32_t BIT_ONE_SPACE_US = 1244;
static const size_t REMOTE_DATA_SIZE = 2 + 2 * 16 + 2 * 32 + 2;

void PanasonicProtocol::encode(RemoteTransmitData *dst, const PanasonicData &data) {
  dst->set_carrier_frequency(35000);
  dst->reserve(REMOTE_DATA_SIZE);
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  encode_data_msb<uint16_t, 16, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, data.address);
  encode_data_msb<uint32_t, 32, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, data.command);
  dst->mark(BIT_MARK_US);
}

static bool decode_data(RemoteReceiveData &src, PanasonicData &dst) {
  return decode_data_msb<uint16_t, 16, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, dst.address) &&
         decode_data_msb<uint32_t, 32, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, dst.command);
}

optional<PanasonicData> PanasonicProtocol::decode(RemoteReceiveData src) {
  PanasonicData out{
      .address = 0,
      .command = 0,
  };
  if (src.has_size(REMOTE_DATA_SIZE) && src.expect_item(HEADER_HIGH_US, HEADER_LOW_US) && decode_data(src, out) &&
      src.expect_mark(BIT_MARK_US))
    return out;
  return {};
}

void PanasonicProtocol::dump(const PanasonicData &data) {
  ESP_LOGD(TAG, "Received Panasonic: address=0x%04X, command=0x%08X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
