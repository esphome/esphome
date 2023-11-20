#include "status_indicator.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include "esphome/components/network/util.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome {
namespace status_indicator {

static const char *const TAG = "status_indicator";

void StatusIndicator::dump_config() { ESP_LOGCONFIG(TAG, "Status Indicator:"); }
void StatusIndicator::loop() {
  std::string status{""};
  if ((App.get_app_state() & STATUS_LED_ERROR) != 0u) {
    status = "on_app_error";
    this->status_.on_error = 1;
  } else if (this->status_.on_error == 1) {
    status = "on_clear_app_error";
    this->status_.on_error = 0;
  } else if ((App.get_app_state() & STATUS_LED_WARNING) != 0u) {
    status = "on_app_warning";
    this->status_.on_warning = 1;
  } else if (this->status_.on_warning == 1) {
    status = "on_clear_app_warning";
    this->status_.on_warning = 0;
  }
  if (this->current_trigger_ != nullptr) {
    if (this->current_trigger_->is_action_running()) {
      if (status == "") {
        return;
      }
      this->current_trigger_->stop_action();
    }
  }
  if (network::has_network()) {
#ifdef USE_WIFI
    if (status == "" && wifi::global_wifi_component->is_ap_enabled()) {
      status = "on_wifi_ap_enabled";
      this->status_.on_wifi_ap = 1;
    } else if (this->status_.on_wifi_ap == 1) {
      status = "on_wifi_ap_disabled";
      this->status_.on_wifi_ap = 0;
    }
#endif

    if (status == "" && not network::is_connected()) {
      status = "on_network_disconnected";
      this->status_.on_network = 1;
    } else if (this->status_.on_network == 1) {
      status = "on_network_connected";
      this->status_.on_network = 0;
    }

#ifdef USE_API
    if (status == "" && api::global_api_server != nullptr && not api::global_api_server->is_connected()) {
      status = "on_api_disconnected";
      this->status_.on_api = 1;
    } else if (this->status_.on_error == 1) {
      status = "on_api_connected";
      this->status_.on_api = 0;
    }
#endif
#ifdef USE_MQTT
    if (status == "" && mqtt::global_mqtt_client != nullptr && not mqtt::global_mqtt_client->is_connected()) {
      status = "on_mqtt_disconnected";
      this->status_.on_mqtt = 1;
    } else if (this->status_.on_mqtt == 1) {
      status = "on_mqtt_connected";
      this->status_.on_mqtt = 0;
    }
#endif
  }
  if (this->current_status_ != status) {
    if (status != "") {
      this->current_trigger_ = get_trigger(status);
      if (this->current_trigger_ != nullptr) {
        this->current_trigger_->trigger();
      }
    } else {
      this->current_trigger_ = nullptr;
      if (!this->custom_triggers_.empty()) {
        this->custom_triggers_[0]->trigger();
      }
    }
    this->current_status_ = status;
  }
}

float StatusIndicator::get_setup_priority() const { return setup_priority::HARDWARE; }
float StatusIndicator::get_loop_priority() const { return 50.0f; }

StatusTrigger *StatusIndicator::get_trigger(std::string key) {
  auto search = this->triggers_.find(key);
  if (search != this->triggers_.end())
    return search->second;
  else {
    return nullptr;
  }
}

void StatusIndicator::set_trigger(std::string key, StatusTrigger *trigger) { this->triggers_[key] = trigger; }

void StatusIndicator::push_trigger(StatusTrigger *trigger) {
  this->pop_trigger(trigger, true);
  uint32_t x = 0;
  while (this->custom_triggers_.size() > x) {
    if (trigger->get_priority() <= this->custom_triggers_[x]->get_priority()) {
      this->custom_triggers_.insert(this->custom_triggers_.begin() + x, trigger);
      break;
    } else {
      x++;
    }
  }
}

void StatusIndicator::pop_trigger(StatusTrigger *trigger, bool incl_group) {
  uint32_t x = 0;
  while (this->custom_triggers_.size() > x) {
    if (trigger == this->custom_triggers_[x]) {
      this->custom_triggers_.erase(this->custom_triggers_.begin() + x);
    } else if (incl_group && trigger->get_group() != "" && trigger->get_group() == this->custom_triggers_[x]->get_group()) {
      this->custom_triggers_.erase(this->custom_triggers_.begin() + x);
    } else {
      x++;
    }
  }
}

void StatusIndicator::pop_trigger(std::string group) {
  uint32_t x = 0;
  while (this->custom_triggers_.size() > x) {
    if ( group == this->custom_triggers_[x]->get_group()) {
      this->custom_triggers_.erase(this->custom_triggers_.begin() + x);
    } else {
      x++;
    }
  }
}

}  // namespace status_indicator
}  // namespace esphome
