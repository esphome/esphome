#include "nextion_textsensor.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nextion {
static const char *const TAG = "nextion_textsensor";

void NextionTextSensor::process_text(const std::string &variable_name, const std::string &text_value) {
  if (!this->nextion_->is_setup())
    return;
  if (this->variable_name_ == variable_name) {
    this->publish_state(text_value);
    ESP_LOGD(TAG, "Processed text_sensor \"%s\" state \"%s\"", variable_name.c_str(), text_value.c_str());
  }
}

void NextionTextSensor::update() {
  if (!this->nextion_->is_setup())
    return;
  this->nextion_->add_to_get_queue(this);
}

void NextionTextSensor::set_state(const std::string &state, bool publish, bool send_to_nextion) {
  if (!this->nextion_->is_setup())
    return;

  if (send_to_nextion) {
    if (this->nextion_->is_sleeping() || !this->visible_) {
      this->needs_to_send_update_ = true;
    } else {
      this->nextion_->add_no_result_to_queue_with_set(this, state);
    }
  }

  if (publish) {
    this->publish_state(state);
  } else {
    this->state = state;
    this->has_state_ = true;
  }

  this->update_component_settings();

  ESP_LOGN(TAG, "Wrote state for text_sensor \"%s\" state \"%s\"", this->variable_name_.c_str(), state.c_str());
}

}  // namespace nextion
}  // namespace esphome
