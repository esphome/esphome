#include "seesaw_encoder.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>

namespace esphome::seesaw {

ESPHOME_LOG_TAG(TAG, "seesaw.encoder");

static constexpr uint32_t POLL_INTERVAL_MS = 50;

void SeesawEncoder::setup() {
  if (seesaw_hub_failed(this, this->parent_))
    return;
  this->parent_->enable_encoder(this->number_);
  int32_t position = 0;
  if (!this->parent_->get_encoder_position(this->number_, position)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->last_position_ = position;
  this->value_ = std::clamp(position, this->min_value_, this->max_value_);
  this->publish_state(this->value_);
}

void SeesawEncoder::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_poll_ < POLL_INTERVAL_MS)
    return;
  this->last_poll_ = now;
  int32_t position = 0;
  if (!this->status_comm_result(this->parent_->get_encoder_position(this->number_, position)))
    return;
  // The movement since the last sample; the chip's position may wrap, the difference does not
  const auto delta =
      static_cast<int32_t>(static_cast<uint32_t>(position) - static_cast<uint32_t>(this->last_position_));
  if (delta == 0)
    return;
  this->last_position_ = position;
  // Clamping the running value, not the chip's position, means turning back from a limit moves at once
  const int32_t value =
      static_cast<int32_t>(std::clamp<int64_t>(int64_t(this->value_) + delta, this->min_value_, this->max_value_));
  if (value == this->value_)
    return;
  this->value_ = value;
  this->publish_state(value);
}

void SeesawEncoder::dump_config() {
  LOG_SENSOR("", "Seesaw Encoder", this);
  ESP_LOGCONFIG(TAG, "  Number: %u", this->number_);
  if (this->min_value_ != INT32_MIN) {
    ESP_LOGCONFIG(TAG, "  Min value: %" PRId32, this->min_value_);
  }
  if (this->max_value_ != INT32_MAX) {
    ESP_LOGCONFIG(TAG, "  Max value: %" PRId32, this->max_value_);
  }
}

}  // namespace esphome::seesaw
