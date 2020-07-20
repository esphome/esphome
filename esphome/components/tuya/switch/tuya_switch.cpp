#include "esphome/core/log.h"
#include "tuya_switch.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.switch";

void TuyaSwitch::setup() {
  this->parent_->register_listener(this->switch_id_, [this](TuyaDatapoint datapoint) {
    this->publish_state(datapoint.value_bool);
    ESP_LOGD(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
  });
}

void TuyaSwitch::write_state(bool state) {
  TuyaDatapoint datapoint{};
  datapoint.id = this->switch_id_;
  datapoint.type = TuyaDatapointType::BOOLEAN;
  datapoint.value_bool = state;
  this->parent_->set_datapoint_value(datapoint);
  ESP_LOGD(TAG, "Setting switch: %s", ONOFF(state));

  this->publish_state(state);
}

void TuyaSwitch::dump_config() {
  LOG_SWITCH("", "Tuya Switch", this);
  ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", this->switch_id_);
}

}  // namespace tuya
}  // namespace esphome
