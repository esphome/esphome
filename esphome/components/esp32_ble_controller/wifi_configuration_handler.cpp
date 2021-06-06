#include "wifi_configuration_handler.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"

namespace esphome {
namespace esp32_ble_controller {

static const char *TAG = "wifi_configuration_handler";

void WifiConfigurationHandler::setup() {
  // Hash with compilation time
  // This ensures the AP override is not applied for OTA
  uint32_t hash = fnv1_hash("wifi_configuration#" + App.get_compilation_time());
  wifi_configuration_preference = global_preferences.make_preference<WifiConfiguration>(hash, true);

  WifiConfiguration configuration;
  if (load_configuration(configuration)) {
    ESP_LOGI(TAG, "Overriding WIFI configuration with stored preferences");
    override_sta(configuration);
  }
}

void WifiConfigurationHandler::set_credentials(const std::string &ssid, const std::string &password, bool hidden_network) {
  ESP_LOGI(TAG, "Updating WIFI configuration");

  WifiConfiguration configuration;

  strncpy(configuration.ssid, ssid.c_str(), WIFI_SSID_LEN);
  strncpy(configuration.password, password.c_str(), WIFI_PASSWORD_LEN);
  configuration.hidden_network = hidden_network;

  if (!save_configuration(configuration)) {
    ESP_LOGE(TAG, "Could not save new WIFI configuration");
  }

  override_sta(configuration);
}

void WifiConfigurationHandler::clear_credentials() {
  ESP_LOGI(TAG, "Clearing WIFI configuration");
    
  WifiConfiguration configuration;
  configuration.ssid[0] = 0;

  if (!save_configuration(configuration)) {
    ESP_LOGE(TAG, "Could not clear WIFI configuration");
  }
}

const optional<std::string> WifiConfigurationHandler::get_current_ssid() const {
  WifiConfiguration configuration;
  if (load_configuration(configuration)) {
    return make_optional<std::string>(configuration.ssid);
  } else {
    return optional<std::string>();
  }
}

bool WifiConfigurationHandler::load_configuration(WifiConfiguration& configuration) const {
  return const_cast<WifiConfigurationHandler*>(this)->wifi_configuration_preference.load(&configuration) && strlen(configuration.ssid);
}

bool WifiConfigurationHandler::save_configuration(const WifiConfiguration& configuration) {
  return wifi_configuration_preference.save(&configuration);
}

void WifiConfigurationHandler::override_sta(const WifiConfiguration& configuration) {
  wifi::WiFiAP sta;

  sta.set_ssid(configuration.ssid);
  sta.set_password(configuration.password);
  sta.set_hidden(configuration.hidden_network);

  wifi::global_wifi_component->set_sta(sta);
}

} // namespace esp32_ble_controller
} // namespace esphome
