#include "toshiba_ac_protocol.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.toshibaac";

static const uint32_t HEADER_HIGH_US = 4500;
static const uint32_t HEADER_LOW_US = 4500;
static const uint32_t BIT_MARK_US = 560;
static const uint32_t BIT_ONE_SPACE_US = 1690;
static const uint32_t BIT_ZERO_SPACE_US = 560;
static const uint32_t FOOTER_HIGH_US = 560;
static const uint32_t FOOTER_LOW_US = 4500;
static const uint16_t PACKET_SPACE = 5500;

void ToshibaAcProtocol::encode(RemoteTransmitData *dst, const ToshibaAcData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(3 * (2 + 2 * 48 + 2));

  for (uint8_t repeat = 0; repeat < 2; repeat++) {
    dst->item(HEADER_HIGH_US, HEADER_LOW_US);
    encode_data_msb<uint64_t, 48, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, data.rc_code_1);
    dst->item(FOOTER_HIGH_US, FOOTER_LOW_US);
  }

  if (data.rc_code_2 != 0) {
    dst->item(HEADER_HIGH_US, HEADER_LOW_US);
    encode_data_msb<uint64_t, 48, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, data.rc_code_2);
    dst->item(FOOTER_HIGH_US, FOOTER_LOW_US);
  }
}

optional<ToshibaAcData> ToshibaAcProtocol::decode(RemoteReceiveData src) {
  uint64_t packet = 0;
  ToshibaAcData out{
      .rc_code_1 = 0,
      .rc_code_2 = 0,
  };
  // *** Packet 1
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};
  if (!decode_data_msb<uint64_t, 48, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, packet))
    return {};
  if (!src.expect_item(FOOTER_HIGH_US, FOOTER_LOW_US))
    return {};

  // *** Packet 2
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};
  if (!decode_data_msb<uint64_t, 48, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, out.rc_code_1))
    return {};
  if (!src.expect_mark(FOOTER_HIGH_US))
    return {};
  // The first two packets must match
  if (packet != out.rc_code_1)
    return {};
  // The third packet isn't always present
  if (!src.expect_space(FOOTER_LOW_US))
    return out;

  // *** Packet 3
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};
  if (!decode_data_msb<uint64_t, 48, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, out.rc_code_2))
    return {};
  if (!src.expect_mark(FOOTER_HIGH_US))
    return {};
  return out;
}

void ToshibaAcProtocol::dump(const ToshibaAcData &data) {
  if (data.rc_code_2 != 0)
    ESP_LOGD(TAG, "Received Toshiba AC: rc_code_1=0x%" PRIX64 ", rc_code_2=0x%" PRIX64, data.rc_code_1, data.rc_code_2);
  else
    ESP_LOGD(TAG, "Received Toshiba AC: rc_code_1=0x%" PRIX64, data.rc_code_1);
}

}  // namespace remote_base
}  // namespace esphome
