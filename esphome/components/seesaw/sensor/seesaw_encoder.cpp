#include "seesaw_encoder.h"
#include "esphome/core/log.h"

namespace esphome::seesaw {

static const char *const TAG = "seesaw.encoder";

void SeesawEncoder::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Seesaw rotary encoder...");
  this->parent_->enable_encoder(this->number_);
  this->publish_state(0);
}

void SeesawEncoder::loop() {
  int32_t new_value = this->parent_->get_encoder_position(this->number_);
  if (new_value < this->min_value_)
    new_value = this->min_value_;
  if (new_value > this->max_value_)
    new_value = this->max_value_;
  if (new_value == this->value_)
    return;
  this->value_ = new_value;
  this->publish_state(new_value);
}

}  // namespace esphome::seesaw
