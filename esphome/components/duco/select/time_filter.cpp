#include "time_filter.h"

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.time_filter");

static constexpr uint8_t TIME_FILTER_ADDRESS = 0x31;
static constexpr uint8_t TIME_FILTER_GROUP = 0x0a;
static constexpr uint16_t TIME_FILTER_DAYS[] = {90, 180, 270, 360};

void DucoTimeFilter::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, TIME_FILTER_ADDRESS, TIME_FILTER_GROUP});
}

void DucoTimeFilter::receive_response(const DucoMessage &message) {
  if (message.size < 5)
    return;

  const uint16_t days = encode_uint16(message.data[4], message.data[3]);
  for (size_t index = 0; index < sizeof(TIME_FILTER_DAYS) / sizeof(TIME_FILTER_DAYS[0]); index++) {
    if (days == TIME_FILTER_DAYS[index]) {
      this->publish_state(index);
      return;
    }
  }
  ESP_LOGV(TAG, "Unknown time filter value %u", days);
}

void DucoTimeFilter::control(size_t index) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "Time filter change rejected: allow_dangerous_controls is disabled");
    if (this->has_state())
      this->publish_state(this->active_index_);
    return;
  }

  const uint16_t days = TIME_FILTER_DAYS[index];
  this->parent_->send(nullptr, FUNCTION_PARAMETER,
                      {0x01, TIME_FILTER_ADDRESS, TIME_FILTER_GROUP, static_cast<uint8_t>(days & 0xFF),
                       static_cast<uint8_t>(days >> 8), 0x00, 0x00});
  // Update state immediately
  this->publish_state(index);
}

}  // namespace esphome::duco
