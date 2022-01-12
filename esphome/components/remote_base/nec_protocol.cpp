#include "nec_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.nec";

static const uint32_t TICK_US = 560;
static const uint32_t HEADER_MARK_US = 16 * TICK_US;
static const uint32_t HEADER_SPACE_US = 8 * TICK_US;
static const uint32_t BIT_MARK_US = 1 * TICK_US;
static const uint32_t BIT_ONE_SPACE_US = 3 * TICK_US;
static const uint32_t BIT_ZERO_SPACE_US = 1 * TICK_US;
static const size_t REMOTE_DATA_SIZE = 2 + 2 * 16 + 2 * 16 + 2;

void NECProtocol::encode(RemoteTransmitData *dst, const NECData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(REMOTE_DATA_SIZE);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  lsb::encode<16, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, data.address);
  lsb::encode<16, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, data.command);
  dst->mark(BIT_MARK_US);
}

static bool decode_data(RemoteReceiveData &src, NECData &dst) {
  return lsb::decode<16, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, dst.address) &&
         lsb::decode<16, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, dst.command);
}

optional<NECData> NECProtocol::decode(RemoteReceiveData src) {
  NECData data{
      .address = 0,
      .command = 0,
  };
  if (src.has_size(REMOTE_DATA_SIZE) && src.expect_item(HEADER_MARK_US, HEADER_SPACE_US) && decode_data(src, data) &&
      src.expect_mark(BIT_MARK_US))
    return data;
  return {};
}

void NECProtocol::dump(const NECData &data) {
  ESP_LOGD(TAG, "Received NEC: address=0x%04X, command=0x%04X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
