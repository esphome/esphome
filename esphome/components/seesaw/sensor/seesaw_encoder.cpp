#include "seesaw_encoder.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::seesaw {

ESPHOME_LOG_TAG(TAG, "seesaw.encoder");

static constexpr uint32_t POLL_INTERVAL_MS = 50;

void SeesawEncoder::setup() {
  this->parent_->enable_encoder(this->number_);
  int32_t position = 0;
  this->parent_->get_encoder_position(this->number_, &position);
  this->value_ = std::clamp(position, this->min_value_, this->max_value_);
  if (this->value_ != position)
    this->parent_->set_encoder_position(this->number_, this->value_);
  this->publish_state(this->value_);
}

void SeesawEncoder::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_poll_ < POLL_INTERVAL_MS)
    return;
  this->last_poll_ = now;
  int32_t position;
  if (!this->parent_->get_encoder_position(this->number_, &position))
    return;
  const int32_t value = std::clamp(position, this->min_value_, this->max_value_);
  // The device keeps counting past a limit; moving it back avoids a dead zone when turning back
  if (value != position)
    this->parent_->set_encoder_position(this->number_, value);
  if (value == this->value_)
    return;
  this->value_ = value;
  this->publish_state(value);
}

void SeesawEncoder::dump_config() {
  LOG_SENSOR("", "Seesaw Encoder", this);
  ESP_LOGCONFIG(TAG, "  Number: %u", this->number_);
}

}  // namespace esphome::seesaw
