#include "toshiba_ac_protocol.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.toshibaac";

static const uint32_t HEADER_HIGH_US = 4500;
static const uint32_t HEADER_LOW_US = 4500;
static const uint32_t BIT_HIGH_US = 560;
static const uint32_t BIT_ONE_LOW_US = 1690;
static const uint32_t BIT_ZERO_LOW_US = 560;
static const uint32_t FOOTER_HIGH_US = 560;
static const uint32_t FOOTER_LOW_US = 4500;
static const uint16_t PACKET_SPACE = 5500;

void ToshibaAcProtocol::encode(RemoteTransmitData *dst, const ToshibaAcData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve((3 + (48 * 2)) * 3);

  for (uint8_t repeat = 0; repeat < 2; repeat++) {
    dst->item(HEADER_HIGH_US, HEADER_LOW_US);
    for (uint8_t bit = 48; bit > 0; bit--) {
      dst->mark(BIT_HIGH_US);
      if ((data.rc_code_1 >> (bit - 1)) & 1) {
        dst->space(BIT_ONE_LOW_US);
      } else {
        dst->space(BIT_ZERO_LOW_US);
      }
    }
    dst->item(FOOTER_HIGH_US, FOOTER_LOW_US);
  }

  if (data.rc_code_2 != 0) {
    dst->item(HEADER_HIGH_US, HEADER_LOW_US);
    for (uint8_t bit = 48; bit > 0; bit--) {
      dst->mark(BIT_HIGH_US);
      if ((data.rc_code_2 >> (bit - 1)) & 1) {
        dst->space(BIT_ONE_LOW_US);
      } else {
        dst->space(BIT_ZERO_LOW_US);
      }
    }
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
  for (uint8_t bit_counter = 0; bit_counter < 48; bit_counter++) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      packet = (packet << 1) | 1;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      packet = (packet << 1) | 0;
    } else {
      return {};
    }
  }
  if (!src.expect_item(FOOTER_HIGH_US, PACKET_SPACE))
    return {};

  // *** Packet 2
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};
  for (uint8_t bit_counter = 0; bit_counter < 48; bit_counter++) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.rc_code_1 = (out.rc_code_1 << 1) | 1;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      out.rc_code_1 = (out.rc_code_1 << 1) | 0;
    } else {
      return {};
    }
  }
  // The first two packets must match
  if (packet != out.rc_code_1)
    return {};
  // The third packet isn't always present
  if (!src.expect_item(FOOTER_HIGH_US, PACKET_SPACE))
    return out;

  // *** Packet 3
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};
  for (uint8_t bit_counter = 0; bit_counter < 48; bit_counter++) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.rc_code_2 = (out.rc_code_2 << 1) | 1;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      out.rc_code_2 = (out.rc_code_2 << 1) | 0;
    } else {
      return {};
    }
  }

  return out;
}

void ToshibaAcProtocol::dump(const ToshibaAcData &data) {
  if (data.rc_code_2 != 0) {
    ESP_LOGD(TAG, "Received Toshiba AC: rc_code_1=0x%" PRIX64 ", rc_code_2=0x%" PRIX64, data.rc_code_1, data.rc_code_2);
  } else {
    ESP_LOGD(TAG, "Received Toshiba AC: rc_code_1=0x%" PRIX64, data.rc_code_1);
  }
}

}  // namespace remote_base
}  // namespace esphome
