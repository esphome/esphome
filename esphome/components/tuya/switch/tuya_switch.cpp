#include "esphome/core/log.h"
#include "tuya_switch.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.switch";

void TuyaSwitch::setup() {
  this->parent_->register_listener(this->switch_id_, [this](const TuyaDatapoint &datapoint) {
    ESP_LOGV(TAG, "MCU reported switch %u is: %s", this->switch_id_, ONOFF(datapoint.value_bool));
    this->publish_state(datapoint.value_bool);
  });
}

void TuyaSwitch::write_state(bool state) {
  ESP_LOGV(TAG, "Setting switch %u: %s", this->switch_id_, ONOFF(state));
  this->parent_->set_boolean_datapoint_value(this->switch_id_, state);
  this->publish_state(state);
}

void TuyaSwitch::dump_config() {
  LOG_SWITCH("", "Tuya Switch", this);
  ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", this->switch_id_);
}

}  // namespace tuya
}  // namespace esphome
