#include "jvc_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.jvc";

static const uint8_t NBITS = 16;
static const uint32_t HEADER_MARK_US = 8400;
static const uint32_t HEADER_SPACE_US = 4200;
static const uint32_t BIT_MARK_US = 525;
static const uint32_t BIT_ONE_SPACE_US = 1725;
static const uint32_t BIT_ZERO_SPACE_US = 525;
static const size_t REMOTE_DATA_SIZE = 2 + 2 * NBITS + 2;

static void encode_data(RemoteTransmitData *dst, const JVCData &src) {
  encode_data_msb<uint32_t, NBITS, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, src.data);
}

void JVCProtocol::encode(RemoteTransmitData *dst, const JVCData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(REMOTE_DATA_SIZE);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  encode_data(dst, data);
  dst->mark(BIT_MARK_US);
}

static bool decode_data(RemoteReceiveData &src, JVCData &dst) {
  return decode_data_msb<uint32_t, NBITS, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, dst.data);
}

optional<JVCData> JVCProtocol::decode(RemoteReceiveData src) {
  JVCData out{
      .data = 0,
  };
  if (src.has_size(REMOTE_DATA_SIZE) && src.expect_item(HEADER_MARK_US, HEADER_SPACE_US) && decode_data(src, out) &&
      src.expect_mark(BIT_MARK_US))
    return out;
  return {};
}

void JVCProtocol::dump(const JVCData &data) { ESP_LOGD(TAG, "Received JVC: data=0x%04X", data.data); }

}  // namespace remote_base
}  // namespace esphome
