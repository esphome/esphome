#include "pioneer_wyt_protocol.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.pioneer_wyt");

// The timings and frame header are shared with tcl112
static constexpr uint32_t HEADER_MARK_US = 3100;
static constexpr uint32_t HEADER_SPACE_US = 1650;
static constexpr uint32_t BIT_MARK_US = 500;
static constexpr uint32_t BIT_ONE_SPACE_US = 1100;
static constexpr uint32_t BIT_ZERO_SPACE_US = 350;
static constexpr BitTimings BIT_TIMINGS{
    .bit_mark = BIT_MARK_US, .one_space = BIT_ONE_SPACE_US, .zero_space = BIT_ZERO_SPACE_US};

void PioneerWytProtocol::encode(RemoteTransmitData *dst, const PioneerWytData &data) {
  char hex_buf[format_hex_pretty_size(PioneerWytData::SIZE)];
  ESP_LOGD(TAG, "Transmit PioneerWyt: %s", format_hex_pretty_to(hex_buf, data.data(), data.size()));
  dst->set_carrier_frequency(38000);
  // header mark and space, a mark and space per bit, then the trailing mark
  dst->reserve(dst->get_data().size() + 3 + PioneerWytData::SIZE * 8 * 2);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  dst->item_bytes<BIT_TIMINGS>(data.data(), data.size());
  dst->mark(BIT_MARK_US);
}

}  // namespace esphome::remote_base
