#include "wifi_info_text_sensor.h"
#ifdef USE_WIFI
#include "esphome/core/log.h"

namespace esphome::wifi_info {

static const char *const TAG = "wifi_info";

#ifdef USE_ESP32
/// @brief Helper function to convert ESP32 WiFi power save mode to string
/// @param ps_mode WiFi power save mode from esp_wifi_get_ps()
/// @return const char pointer to the readable power save mode
///
/// Maps ESP32 WiFi power save modes to user-friendly strings:
/// - WIFI_PS_NONE (no power saving) -> "NONE"
/// - WIFI_PS_MIN_MODEM (minimal modem sleep) -> "LIGHT"
/// - WIFI_PS_MAX_MODEM (maximum modem sleep) -> "HIGH"
static const char *wifi_ps_mode_to_string(wifi_ps_type_t ps_mode) {
  switch (ps_mode) {
    case WIFI_PS_NONE:
      return "NONE";
    case WIFI_PS_MIN_MODEM:
      return "LIGHT";
    case WIFI_PS_MAX_MODEM:
      return "HIGH";
    default:
      return "UNKNOWN";
  }
}
#endif  // USE_ESP32

#ifdef USE_WIFI_LISTENERS

static constexpr size_t MAX_STATE_LENGTH = 255;

/********************
 * IPAddressWiFiInfo
 *******************/

void IPAddressWiFiInfo::setup() { wifi::global_wifi_component->add_ip_state_listener(this); }

void IPAddressWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "IP Address", this); }

void IPAddressWiFiInfo::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                                    const network::IPAddress &dns2) {
  this->publish_state(ips[0].str());
  uint8_t sensor = 0;
  for (const auto &ip : ips) {
    if (ip.is_set()) {
      if (this->ip_sensors_[sensor] != nullptr) {
        this->ip_sensors_[sensor]->publish_state(ip.str());
      }
      sensor++;
    }
  }
}

/*********************
 * DNSAddressWifiInfo
 ********************/

void DNSAddressWifiInfo::setup() { wifi::global_wifi_component->add_ip_state_listener(this); }

void DNSAddressWifiInfo::dump_config() { LOG_TEXT_SENSOR("", "DNS Address", this); }

void DNSAddressWifiInfo::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                                     const network::IPAddress &dns2) {
  std::string dns_results = dns1.str() + " " + dns2.str();
  this->publish_state(dns_results);
}

/**********************
 * ScanResultsWiFiInfo
 *********************/

void ScanResultsWiFiInfo::setup() { wifi::global_wifi_component->add_scan_results_listener(this); }

void ScanResultsWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "Scan Results", this); }

void ScanResultsWiFiInfo::on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult> &results) {
  std::string scan_results;
  for (const auto &scan : results) {
    if (scan.get_is_hidden())
      continue;

    scan_results += scan.get_ssid();
    scan_results += ": ";
    scan_results += esphome::to_string(scan.get_rssi());
    scan_results += "dB\n";
  }
  // There's a limit of 255 characters per state; longer states just don't get sent so we truncate it
  if (scan_results.length() > MAX_STATE_LENGTH) {
    scan_results.resize(MAX_STATE_LENGTH);
  }
  this->publish_state(scan_results);
}

/***************
 * SSIDWiFiInfo
 **************/

void SSIDWiFiInfo::setup() { wifi::global_wifi_component->add_connect_state_listener(this); }

void SSIDWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "SSID", this); }

void SSIDWiFiInfo::on_wifi_connect_state(const std::string &ssid, const wifi::bssid_t &bssid) {
  this->publish_state(ssid);
}

/****************
 * BSSIDWiFiInfo
 ***************/

void BSSIDWiFiInfo::setup() { wifi::global_wifi_component->add_connect_state_listener(this); }

void BSSIDWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "BSSID", this); }

void BSSIDWiFiInfo::on_wifi_connect_state(const std::string &ssid, const wifi::bssid_t &bssid) {
  char buf[18] = "unknown";
  if (mac_address_is_valid(bssid.data())) {
    format_mac_addr_upper(bssid.data(), buf);
  }
  this->publish_state(buf);
}

#endif

/*********************
 * MacAddressWifiInfo
 ********************/

void MacAddressWifiInfo::dump_config() { LOG_TEXT_SENSOR("", "MAC Address", this); }

#ifdef USE_ESP32
void PowerSaveModeWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "WiFi Power Save Mode", this); }

void PowerSaveModeWiFiInfo::update() {
  wifi_ps_type_t power_save_mode;
  if (esp_wifi_get_ps(&power_save_mode) == ESP_OK) {
    // Publish if the state has changed or if this is the first read
    if (this->last_power_save_mode_ != power_save_mode || !this->has_state()) {
      this->publish_state(wifi_ps_mode_to_string(power_save_mode));
      this->last_power_save_mode_ = power_save_mode;
    }
  }
}
#endif  // USE_ESP32

}  // namespace esphome::wifi_info
#endif
