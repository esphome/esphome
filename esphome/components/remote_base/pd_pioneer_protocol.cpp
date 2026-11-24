#include "pd_pioneer_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.pd_pioneer");

// Timings from captured Pronto codes (006D timebase, 26 us per word)
static constexpr uint32_t HEADER_MARK_US = 3120;
static constexpr uint32_t HEADER_SPACE_US = 1560;
static constexpr uint32_t BIT_MARK_US = 520;
static constexpr uint32_t BIT_ONE_SPACE_US = 1066;
static constexpr uint32_t BIT_ZERO_SPACE_US = 312;
static constexpr uint32_t FOOTER_MARK_US = 520;
static constexpr uint32_t FOOTER_SPACE_US = 10010;

void PDPioneerProtocol::encode(RemoteTransmitData *dst, const PDPioneerData &src) {
  dst->set_carrier_frequency(38000);
  dst->reserve(2 + PDPioneerData::FRAME_SIZE * 8 * 2 + 2);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  for (uint8_t idx = 0; idx < PDPioneerData::FRAME_SIZE; idx++) {
    for (uint8_t mask = 1; mask; mask <<= 1)
      dst->item(BIT_MARK_US, (src[idx] & mask) ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
  }
  dst->item(FOOTER_MARK_US, FOOTER_SPACE_US);
}

optional<PDPioneerData> PDPioneerProtocol::decode(RemoteReceiveData src) {
  PDPioneerData out;
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
    return {};
  for (uint8_t idx = 0; idx < PDPioneerData::FRAME_SIZE; idx++) {
    for (uint8_t mask = 1; mask; mask <<= 1) {
      if (src.expect_item(BIT_MARK_US, BIT_ONE_SPACE_US)) {
        out[idx] |= mask;
      } else if (!src.expect_item(BIT_MARK_US, BIT_ZERO_SPACE_US)) {
        return {};
      }
    }
  }
  // The receiver's idle timeout usually ends the capture inside the footer space, so only the mark is required
  if (!src.expect_mark(FOOTER_MARK_US))
    return {};
  if (!out.is_valid()) {
    char buf[PDPioneerData::TO_STR_BUFFER_SIZE];
    ESP_LOGV(TAG, "Checksum mismatch: %s", out.to_str(buf));
    return {};
  }
  return out;
}

void PDPioneerProtocol::dump(const PDPioneerData &data) {
  char buf[PDPioneerData::TO_STR_BUFFER_SIZE];
  ESP_LOGI(TAG, "Received PD-Pioneer: %s", data.to_str(buf));
}

}  // namespace esphome::remote_base
