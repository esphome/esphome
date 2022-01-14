#include "toshiba_ac_protocol.h"
#include "helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.toshibaac";

static const uint32_t HEADER_MARK_US = 4500;
static const uint32_t HEADER_SPACE_US = 4500;
static const uint32_t BIT_MARK_US = 560;
static const uint32_t BIT_ONE_SPACE_US = 1690;
static const uint32_t BIT_ZERO_SPACE_US = 560;
static const uint32_t FOOTER_MARK_US = 560;
static const uint32_t FOOTER_SPACE_US = 4500;
static const uint16_t PACKET_SPACE = 5500;

USE_SPACE_MSB_CODEC(codec)

void ToshibaAcProtocol::encode(RemoteTransmitData *dst, const ToshibaAcData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(3 * (2 + 2 * 48 + 2));

  for (uint8_t repeat = 0; repeat < 2; repeat++) {
    dst->item(HEADER_MARK_US, HEADER_SPACE_US);
    codec::encode(dst, data.rc_code_1, 48);
    dst->item(FOOTER_MARK_US, FOOTER_SPACE_US);
  }

  if (data.rc_code_2 != 0) {
    dst->item(HEADER_MARK_US, HEADER_SPACE_US);
    codec::encode(dst, data.rc_code_2, 48);
    dst->item(FOOTER_MARK_US, FOOTER_SPACE_US);
  }
}

static bool decode_data(RemoteReceiveData &src, uint64_t &dst) {
  return codec::decode(src, dst, 48) == 48;
}

optional<ToshibaAcData> ToshibaAcProtocol::decode(RemoteReceiveData src) {
  uint64_t packet = 0;
  ToshibaAcData out{
      .rc_code_1 = 0,
      .rc_code_2 = 0,
  };
  // *** Packet 1
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
    return {};
  if (!decode_data(src, packet))
    return {};
  if (!src.expect_item(FOOTER_MARK_US, FOOTER_SPACE_US))
    return {};

  // *** Packet 2
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
    return {};
  if (!decode_data(src, out.rc_code_1))
    return {};
  if (!src.expect_mark(FOOTER_MARK_US))
    return {};
  // The first two packets must match
  if (packet != out.rc_code_1)
    return {};
  // The third packet isn't always present
  if (!src.expect_space(FOOTER_SPACE_US))
    return out;

  // *** Packet 3
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
    return {};
  if (!decode_data(src, out.rc_code_2))
    return {};
  if (!src.expect_mark(FOOTER_MARK_US))
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
