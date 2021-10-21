#include "nextion_switch.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nextion {

static const char *const TAG = "nextion_switch";

void NextionSwitch::process_bool(const std::string &variable_name, bool on) {
  if (!this->nextion_->is_setup())
    return;
  if (this->variable_name_ == variable_name) {
    this->publish_state(on);

    ESP_LOGD(TAG, "Processed switch \"%s\" state %s", variable_name.c_str(), state ? "ON" : "OFF");
  }
}

void NextionSwitch::update() {
  if (!this->nextion_->is_setup())
    return;
  this->nextion_->add_to_get_queue(this);
}

void NextionSwitch::set_state(bool state, bool publish, bool send_to_nextion) {
  if (!this->nextion_->is_setup())
    return;

  if (send_to_nextion) {
    if (this->nextion_->is_sleeping() || !this->visible_) {
      this->needs_to_send_update_ = true;
    } else {
      this->needs_to_send_update_ = false;
      this->nextion_->add_no_result_to_queue_with_set(this, (int) state);
    }
  }
  if (publish) {
    this->publish_state(state);
  } else {
    this->state = state;
  }

  this->update_component_settings();

  ESP_LOGN(TAG, "Updated switch \"%s\" state %s", this->variable_name_.c_str(), ONOFF(state));
}

void NextionSwitch::write_state(bool state) { this->set_state(state); }

}  // namespace nextion
}  // namespace esphome
