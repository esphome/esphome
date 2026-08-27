#include "wifi_component.h"

#include "esphome/core/log.h"

namespace esphome::wifi {

static const char *const TAG = "wifi_stub";

WiFiComponent *global_wifi_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

WiFiComponent::WiFiComponent() { global_wifi_component = this; }

void WiFiComponent::setup() { ESP_LOGI(TAG, "Stub wifi ready"); }

void WiFiComponent::dump_config() { ESP_LOGCONFIG(TAG, "Stub wifi"); }

void WiFiComponent::start_scanning() {
  // Duplicate TestNet entry (weaker) and a hidden entry exercise the
  // should_show_scan_entry dedup and filtering logic
  this->scan_result_.clear();
  this->scan_result_.emplace_back("TestNet", -50, true, false);
  this->scan_result_.emplace_back("TestNet", -60, true, false);
  this->scan_result_.emplace_back("OpenNet", -70, false, false);
  this->scan_result_.emplace_back("", -40, false, true);
  ESP_LOGI(TAG, "Scan complete with %zu results", this->scan_result_.size());
}

void WiFiComponent::set_sta(const WiFiAP &ap) { ESP_LOGI(TAG, "set_sta ssid=%s", ap.get_ssid().c_str()); }

void WiFiComponent::start_connecting(const WiFiAP &ap) {
  ESP_LOGI(TAG, "start_connecting ssid=%s", ap.get_ssid().c_str());
}

void WiFiComponent::clear_sta() { ESP_LOGI(TAG, "clear_sta"); }

void WiFiComponent::save_wifi_sta(StringRef ssid, StringRef password) {
  ESP_LOGI(TAG, "save_wifi_sta ssid=%s password_len=%zu", ssid.c_str(), password.size());
}

}  // namespace esphome::wifi
