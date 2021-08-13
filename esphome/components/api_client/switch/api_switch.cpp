#include "esphome/core/log.h"
#include "api_switch.h"
#include "../../api/api_pb2.h"

namespace esphome {
namespace api {

static const char *TAG = "apiclient.switch";

void ApiClientSwitch::write_state(bool state) {
  ESP_LOGV(TAG, "Setting switch %u: %s", this->key_, ONOFF(state));
  SwitchCommandRequest req;
  req.key = key_;
  req.state = state;
  if (!this->parent_->send_switch_command_request(req))
    ESP_LOGE(TAG, "Unable to send switch_command_request");
  else
    this->publish_state(state);
}

void ApiClientSwitch::dump_config() {
  LOG_SWITCH("", "API Client Switch", this);
  ESP_LOGCONFIG(TAG, "  Switch has key ID %u", this->key_);
}

}  // namespace api
}  // namespace esphome
