#include "pd_pioneer_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote.pd_pioneer";

// Timings derived from captured Pronto codes (006D timebase → 26 µs/word).
// Must match what remote_base::ProntoProtocol transmits for 0014/0029/000C pairs.
static const int32_t HEADER_MARK_US = 3120;
static const int32_t HEADER_SPACE_US = 1560;
static const int32_t BIT_PULSE_US = 520;
static const int32_t BIT_ONE_SPACE_US = 1066;
static const int32_t BIT_ZERO_SPACE_US = 312;
static const int32_t FOOTER_MARK_US = 520;
static const int32_t FOOTER_SPACE_US = 10010;

static void encode_bit(RemoteTransmitData *dst, bool bit) {
  dst->item(BIT_PULSE_US, bit ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
}

static bool decode_bit(RemoteReceiveData &src, bool *bit) {
  if (!src.is_valid(1))
    return false;
  int32_t mark = src.peek(0);
  int32_t space = src.peek(1);
  src.advance(2);
  *bit = mark < -space;
  return true;
}

void PDPioneerProtocol::encode(RemoteTransmitData *dst, const PDPioneerData &src) {
  char buf[PDPioneerData::TO_STR_BUFFER_SIZE];
  ESP_LOGD(TAG, "encode %s", src.to_str(buf));
  dst->set_carrier_frequency(38000);
  // header + 14 bytes (112 bits) + footer — no start/stop bits (matches Pronto captures)
  dst->reserve(2 + PDPioneerData::FRAME_SIZE * 8 * 2 + 2);

  dst->item(HEADER_MARK_US, HEADER_SPACE_US);

  for (uint8_t idx = 0; idx < PDPioneerData::FRAME_SIZE; idx++) {
    for (uint8_t mask = 1; mask; mask <<= 1)
      encode_bit(dst, (src[idx] & mask) != 0);
  }

  dst->item(FOOTER_MARK_US, FOOTER_SPACE_US);
}

static bool decode_frame(RemoteReceiveData &src, PDPioneerData &dst) {
  bool bit;

  for (uint8_t idx = 0; idx < PDPioneerData::FRAME_SIZE; idx++) {
    uint8_t data = 0;
    for (uint8_t mask = 1; mask; mask <<= 1) {
      if (!decode_bit(src, &bit))
        return false;
      if (bit)
        data |= mask;
    }
    dst[idx] = data;
  }

  return true;
}

optional<PDPioneerData> PDPioneerProtocol::decode(RemoteReceiveData src) {
  PDPioneerData out;

  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
    return {};

  if (!decode_frame(src, out))
    return {};

  if (!src.peek_mark_at_least(FOOTER_MARK_US))
    return {};
  src.advance(1);

  if (!src.peek_space_at_most(-FOOTER_SPACE_US))
    return {};
  src.advance(1);

  if (!out.is_valid()) {
    char buf[PDPioneerData::TO_STR_BUFFER_SIZE];
    ESP_LOGD(TAG, "checksum fail %s", out.to_str(buf));
    return {};
  }

  char buf[PDPioneerData::TO_STR_BUFFER_SIZE];
  ESP_LOGI(TAG, "RX %s burst: %s", out.is_odd_burst() ? "odd" : "even", out.to_str(buf));
  return out;
}

void PDPioneerProtocol::dump(const PDPioneerData &data) {
  char buf[PDPioneerData::TO_STR_BUFFER_SIZE];
  ESP_LOGI(TAG, "Received PD-Pioneer: %s", data.to_str(buf));
}

}  // namespace esphome::remote_base
