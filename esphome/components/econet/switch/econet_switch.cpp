#include "esphome/core/log.h"
#include "econet_switch.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.switch";

void EconetSwitch::setup() {
  this->parent_->register_listener(
      this->switch_id_, this->request_mod_, this->request_once_, [this](const EconetDatapoint &datapoint) {
        ESP_LOGV(TAG, "MCU reported switch %s is: %s", this->switch_id_.c_str(), ONOFF(datapoint.value_enum));
        this->publish_state(datapoint.value_enum);
      });
}

void EconetSwitch::write_state(bool state) {
  ESP_LOGV(TAG, "Setting switch %s: %s", this->switch_id_.c_str(), ONOFF(state));
  this->parent_->set_enum_datapoint_value(this->switch_id_, state);
  this->publish_state(state);
}

void EconetSwitch::dump_config() {
  LOG_SWITCH("", "Econet Switch", this);
  ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %s", this->switch_id_.c_str());
}

}  // namespace econet
}  // namespace esphome
