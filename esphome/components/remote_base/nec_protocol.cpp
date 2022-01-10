#include "nec_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.nec";

static const int32_t TICK_US = 560;
static const int32_t HEADER_MARK_US = 16 * TICK_US;
static const int32_t HEADER_SPACE_US = 8 * TICK_US;
static const int32_t BIT_MARK_US = 1 * TICK_US;
static const int32_t BIT_ONE_SPACE_US = 3 * TICK_US;
static const int32_t BIT_ZERO_SPACE_US = 1 * TICK_US;
static const size_t REMOTE_DATA_SIZE = 2 + 2 * 16 + 2 * 16 + 2;

static void encode_data(RemoteTransmitData *dst, uint16_t src) {
  for (uint16_t mask = 1 << 0; mask; mask <<= 1)
    dst->item(BIT_MARK_US, (src & mask) ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
}

void NECProtocol::encode(RemoteTransmitData *dst, const NECData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(REMOTE_DATA_SIZE);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  encode_data(dst, data.address);
  encode_data(dst, data.command);
  dst->mark(BIT_MARK_US);
}

static bool decode_data(RemoteReceiveData &src, uint16_t &dst) {
  uint16_t data = 0;
  for (uint16_t mask = 1 << 0; mask; mask <<= 1) {
    if (!src.expect_mark(BIT_MARK_US))
      return false;
    if (src.expect_space(BIT_ONE_SPACE_US))
      data |= mask;
    else if (!src.expect_space(BIT_ZERO_SPACE_US))
      return false;
  }
  dst = data;
  return true;
}

optional<NECData> NECProtocol::decode(RemoteReceiveData src) {
  NECData data;
  if (src.has_size(REMOTE_DATA_SIZE) && src.expect_item(HEADER_MARK_US, HEADER_SPACE_US) &&
      decode_data(src, data.address) && decode_data(src, data.command) && src.expect_mark(BIT_MARK_US))
    return data;
  return {};
}

void NECProtocol::dump(const NECData &data) {
  ESP_LOGD(TAG, "Received NEC: address=0x%04X, command=0x%04X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
