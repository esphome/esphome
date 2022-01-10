#include "coolix_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.coolix";

static const uint32_t TICK_US = 560;
static const uint32_t HEADER_MARK_US = 8 * TICK_US;
static const uint32_t HEADER_SPACE_US = 8 * TICK_US;
static const uint32_t BIT_MARK_US = 1 * TICK_US;
static const uint32_t BIT_ONE_SPACE_US = 3 * TICK_US;
static const uint32_t BIT_ZERO_SPACE_US = 1 * TICK_US;
static const uint32_t FOOTER_MARK_US = 1 * TICK_US;
static const uint32_t FOOTER_SPACE_US = 10 * TICK_US;
static const size_t REMOTE_DATA_SIZE = 2 + 2 * 48 + 2 + 2 + 2 * 48 + 2;

static void encode_data(RemoteTransmitData *dst, const CoolixData &src) {
  //   Break data into bytes, starting at the Most Significant
  //   Byte. Each byte then being sent normal, then followed inverted.
  for (unsigned shift = 16;; shift -= 8) {
    // Grab a bytes worth of data.
    const uint8_t byte = src >> shift;
    // Normal
    encode_data_msb<uint8_t, 8, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, byte);
    // Inverted
    encode_data_msb<uint8_t, 8, BIT_MARK_US, BIT_ZERO_SPACE_US, BIT_ONE_SPACE_US>(dst, byte);
    // Data end
    if (shift == 0)
      break;
  }
}

void CoolixProtocol::encode(RemoteTransmitData *dst, const CoolixData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(REMOTE_DATA_SIZE);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  encode_data(dst, data);
  dst->item(FOOTER_MARK_US, FOOTER_SPACE_US);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  encode_data(dst, data);
  dst->mark(FOOTER_MARK_US);
}

static bool decode_data(RemoteReceiveData &src, CoolixData &dst) {
  uint32_t data = 0;
  for (unsigned n = 3;; data <<= 8) {
    // Read byte
    if (!decode_data_msb<uint32_t, 8, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, data))
      return false;
    // Check for inverse byte
    if (!equal_data_msb<uint32_t, 8, BIT_MARK_US, BIT_ZERO_SPACE_US, BIT_ONE_SPACE_US>(src, data))
      return false;
    // Checking the end of reading
    if (--n == 0) {
      dst = data;
      return true;
    }
  }
}

optional<CoolixData> CoolixProtocol::decode(RemoteReceiveData data) {
  CoolixData first, second;
  if (data.has_size(REMOTE_DATA_SIZE) && data.expect_item(HEADER_MARK_US, HEADER_SPACE_US) &&
      decode_data(data, first) && data.expect_item(FOOTER_MARK_US, FOOTER_SPACE_US) &&
      data.expect_item(HEADER_MARK_US, HEADER_SPACE_US) && decode_data(data, second) &&
      data.expect_mark(FOOTER_MARK_US) && first == second)
    return first;
  return {};
}

void CoolixProtocol::dump(const CoolixData &data) { ESP_LOGD(TAG, "Received Coolix: 0x%06X", data); }

}  // namespace remote_base
}  // namespace esphome
