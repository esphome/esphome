#include "status_indicator.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ETHERNET
#include "esphome/components/ethernet/ethernet_component.h"
#endif
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

bool has_network() {
#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr)
    return true;
#endif

#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return true;
#endif

#ifdef USE_HOST
  return true;  // Assume its connected
#endif
  return false;
}

bool is_connected() {
#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr && ethernet::global_eth_component->is_connected())
    return true;
#endif

#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->is_connected();
#endif

#ifdef USE_HOST
  return true;  // Assume its connected
#endif
  return false;
}

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
      if (status.empty()) {
        return;
      }
      this->current_trigger_->stop_action();
    }
  }
  if (has_network()) {
#ifdef USE_WIFI
    if (status.empty() && wifi::global_wifi_component->is_ap_enabled()) {
      status = "on_wifi_ap_enabled";
      this->status_.on_wifi_ap = 1;
    } else if (this->status_.on_wifi_ap == 1) {
      status = "on_wifi_ap_disabled";
      this->status_.on_wifi_ap = 0;
    }
#endif

    if (status.empty() && not is_connected()) {
      status = "on_network_disconnected";
      this->status_.on_network = 1;
    } else if (this->status_.on_network == 1) {
      status = "on_network_connected";
      this->status_.on_network = 0;
    }

#ifdef USE_API
    if (status.empty() && api::global_api_server != nullptr && not api::global_api_server->is_connected()) {
      status = "on_api_disconnected";
      this->status_.on_api = 1;
    } else if (this->status_.on_error == 1) {
      status = "on_api_connected";
      this->status_.on_api = 0;
    }
#endif
#ifdef USE_MQTT
    if (status.empty() && mqtt::global_mqtt_client != nullptr && not mqtt::global_mqtt_client->is_connected()) {
      status = "on_mqtt_disconnected";
      this->status_.on_mqtt = 1;
    } else if (this->status_.on_mqtt == 1) {
      status = "on_mqtt_connected";
      this->status_.on_mqtt = 0;
    }
#endif
  }
  if (this->current_status_ != status) {
    StatusTrigger *oldtrigger = this->current_trigger_;
    if (!status.empty()) {
      this->current_trigger_ = get_trigger(status);
      if (this->current_trigger_ != nullptr) {
        this->current_trigger_ = get_trigger("on_turn_off");
      }
    } else if (!this->stack_.empty()) {
      this->current_trigger_ = this->stack_.back();
    } else {
      this->current_trigger_ = get_trigger("on_turn_off");
    }
    if (oldtrigger != this->current_trigger_) {
      ESP_LOGD(TAG, "Current trigger: %s", this->current_trigger_->get_info().c_str());
      this->current_trigger_->trigger();
    }
    this->current_status_ = status;
  }
}

float StatusIndicator::get_setup_priority() const { return setup_priority::HARDWARE; }
float StatusIndicator::get_loop_priority() const { return 50.0f; }

StatusTrigger *StatusIndicator::get_trigger(const std::string &key) {
  auto search = this->triggers_.find(key);
  if (search != this->triggers_.end()) {
    return search->second;
  } else {
    return nullptr;
  }
}

void StatusIndicator::set_trigger(const std::string &key, StatusTrigger *trigger) { this->triggers_[key] = trigger; }

void StatusIndicator::push_trigger(StatusTrigger *trigger) {
  this->pop_trigger(trigger, true);
  ESP_LOGD(TAG, "Push ID: %s", trigger->get_info().c_str());

  for (auto i = this->stack_.begin(); i != this->stack_.end(); ++i) {
    StatusTrigger *st = *i;
    if (trigger->get_priority() < st->get_priority()) {
      this->stack_.insert(i, trigger);
      this->current_status_ = "update me";
      log_triggers_();
      return;
    }
  }
  this->stack_.push_back(trigger);
  this->current_status_ = "update me";
  log_triggers_();
}

void StatusIndicator::pop_trigger(StatusTrigger *trigger, bool incl_group) {
  incl_group = incl_group && !trigger->get_group().empty();
  ESP_LOGD(TAG, "Pop by ID: %s || %s", trigger->get_info().c_str(), YESNO(incl_group));
  std::string group = trigger->get_group();
  for (auto i = this->stack_.begin(); i != this->stack_.end();) {
    StatusTrigger *st = *i;
    if ((incl_group && group == st->get_group()) || (trigger == st)) {
      this->stack_.erase(i);
      this->current_status_ = "update me";
    } else {
      ++i;
    }
  }
  log_triggers_();
}

void StatusIndicator::pop_trigger(const std::string &group) {
  ESP_LOGD(TAG, "Pop by group: %s", group.c_str());

  for (auto i = this->stack_.begin(); i != this->stack_.end();) {
    StatusTrigger *st = *i;
    if (group == st->get_group()) {
      this->stack_.erase(i);
      this->current_status_ = "update me";
    } else {
      ++i;
    }
  }
  log_triggers_();
}

void StatusIndicator::log_triggers_() {
  for (auto i = this->stack_.begin(); i != this->stack_.end(); ++i) {
    StatusTrigger *st = *i;
    ESP_LOGD(TAG, "%s", st->get_info().c_str());
  }
  ESP_LOGD(TAG, "----------------------------- %d ----", this->stack_.size());
}

}  // namespace status_indicator
}  // namespace esphome
